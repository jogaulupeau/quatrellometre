/* =====================================================================
   QUATRELLOMÈTRE — cadran rétro 4L (ESP32 + GC9A01)
   ---------------------------------------------------------------------
   RPM en aiguille (0–7000 tr/min, zone rouge dès 5500) — c'est la valeur
   qu'on surveille en continu en conduisant, l'aiguille s'y prête mieux.
   Température liquide + tension batterie en digital avec code couleur.

   Mesures réelles : DS18B20 (température), ADS1115 (tension batterie).
   RPM encore simulé — en attente du câblage opto 4N35 sur la bobine.
   ===================================================================== */

#include <Arduino_GFX_Library.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Preferences.h>
#include <math.h>
#include "esp_task_wdt.h"             // chien de garde : reboot auto si loop() se fige

// ---- Debug série (mettre DEBUG à 0 pour la version embarquée définitive) ----
#define DEBUG 1
#if DEBUG
  #define DBG_PRINT(...)   webLog.print(__VA_ARGS__)
  #define DBG_PRINTLN(...) webLog.println(__VA_ARGS__)
#else
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
#endif

// ---- Interface web déportée (smartphone) : AP WiFi + page HTML embarquée ----
// Mettre ENABLE_WEB à 0 pour compiler sans WiFi (utile pour isoler tout impact sur le rendu).
#define ENABLE_WEB 1
#define FW_VERSION "v2"   // marqueur affiché dans l'en-tête web — bumper pour vérifier qu'un OTA a pris
#if ENABLE_WEB
  #include <WiFi.h>
  #include <WebServer.h>
  #include <DNSServer.h>
  #include <Update.h>                  // OTA : flash du firmware via la page web
  #include "secrets.h"                 // AP_PASS / CFG_TOKEN reels (hors source partagee)
  const char* AP_SSID = "4L-Telemetrie";
  const char* AP_PASS   = AP_PASS_SECRET;   // valeur reelle dans secrets.h
  const char* CFG_TOKEN = CFG_TOKEN_SECRET; // valeur reelle dans secrets.h
  WebServer webServer(80);
  DNSServer dnsServer;                  // portail captif : toute requête DNS → 192.168.4.1
#endif

// ---- Trigo en simple précision : reste sur le FPU matériel de l'ESP32 ----
static const float DEG2RAD   = 0.01745329252f;
static const float HALF_PI_F = 1.57079632679f;

#define BLACK     0x0000
#define WHITE     0xFFFF
#define RED       0xF800
#define GREEN     0x07E0
#define ORANGE    0xFD20
#define AMBER     0xFCA0

// ---- Palette rétro fond crème ----
#define COL_BG      0xDE54   // parcheminé  #D8C8A0
#define INK         0x2924   // brun très foncé  #2B2620
#define WARM_BROWN  0x5A46   // brun moyen  #5A4A32
#define RUST_COL    0xC183   // rouille  #C0301C
#define SEPARATOR   0xC591   // beige  #C2B18E
#define PIVOT_OUT   0x4228   // gris foncé
#define DARKGREEN   0x0320   // vert foncé pour lisibilité sur fond clair
#define ZONE_GREEN  0x03E0   // vert sombre des zones de l'arc (fond + restore)

#define PIN_TFT_SCK   18
#define PIN_TFT_MOSI  23
#define PIN_TFT_MISO  19
#define PIN_TFT_CS     5
#define PIN_TFT_DC    16
#define PIN_TFT_RST   17
#define PIN_TFT_BL     4
#define PIN_ONEWIRE   25   // DS18B20 DATA (+ pull-up 4,7kΩ vers 3V3)
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22
#define PIN_RPM       27   // sortie opto 4N35 (collecteur), INPUT_PULLUP, actif bas

#define CX  120
#define CY  120

#define ARC_R_OUTER    102
#define ARC_R_INNER     90
#define NEEDLE_R        80
#define NEEDLE_BACK     18
#define ARC_START_DEG 150.0f
#define ARC_SPAN_DEG  240.0f

// ---- Échelle RPM (aiguille) ----
#define RPM_MIN        0.0f
#define RPM_MAX     7000.0f
#define RPM_OK_MAX  4000.0f   // zone verte jusqu'à 4000
#define RPM_WARN_MAX 5500.0f  // zone orange 4000-5500, rouge au-delà

// ---- Source RPM : 0 = simulation, 1 = capture réelle GPIO27 (opto 4N35) ----
#define RPM_SOURCE_REAL 1

// Multiplicateur de TEST au doigt (capture réelle) : 1 = normal. Mets ~20-30 pour qu'une
// impulsion manuelle lente fasse réagir l'aiguille. ⚠ REMETTRE À 1 pour la vraie bobine !
#define RPM_TEST_MULT 1

// ---- Seuils température (affichage digital) ----
#define TEMP_OK_MAX    95.0f
#define TEMP_WARN_MAX 105.0f

// ---- Seuils tension batterie : zone rouge = alarme (partagés affichage + anneau) ----
#define VOLT_RED_LOW   11.8f
#define VOLT_RED_HIGH  15.0f

// ---- Graduations RPM (source unique : trait + label) ----
const float       gradRpm[8]    = {0, 1000, 2000, 3000, 4000, 5000, 6000, 7000};
const char* const gradLabels[8] = {"0","1","2","3","4","5","6","7"};

// Horloge SPI volontairement basse = fiabilité sur breadboard (fils longs, pas de masse
// franche). Le défaut de la lib (~40 MHz) génère des artefacts sur câblage non blindé.
// À remonter une fois sur Veroboard si tout est stable.
#define GFX_SPI_SPEED 20000000   // 20 MHz (descendre à 10-12 MHz si les artefacts persistent)
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_MISO);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, PIN_TFT_RST, 0, true);

// ---- DS18B20 (température liquide) ----
OneWire           oneWire(PIN_ONEWIRE);
DallasTemperature ds(&oneWire);
bool     dsFound     = false;
bool     tempValid   = false;   // false = sonde absente/déconnectée → affiche "--"
float    realTemp    = 0.0f;
uint32_t lastTempReq = 0;
const uint32_t TEMP_CONV_MS = 400;
uint32_t lastDsScan = 0;
const uint32_t DS_RESCAN_MS = 3000;  // re-détection sonde si absente au boot (sans reboot)

// ---- ADS1115 (tension batterie) ----
Adafruit_ADS1115 ads;
bool  adsFound = false;
float realVolt = 0.0f;
const float R1_OHMS   = 100000.0f;
const float R2_OHMS   = 18000.0f;
const float DIV_RATIO = R2_OHMS / (R1_OHMS + R2_OHMS);   // ≈ 0,1525
// Calibré le : transfo 12,67V réel (multimètre) → 12,9V affiché → coeff = 12,67/12,9
const float VOLT_CAL  = 0.982f;
uint32_t lastVoltRead = 0;
const uint32_t VOLT_READ_MS = 200;
bool  voltSeeded = false;            // EMA amorcée à la 1re lecture
const float VOLT_EMA_ALPHA = 0.25f;  // filtre : ~1 s de constante de temps à 200 ms/lecture

// ---- Réglages configurables (défauts = #define ci-dessus, surchargés par NVS via la page web) ----
#define BL_LEDC_CH 0                  // canal LEDC pour le PWM du rétroéclairage (cores < 3.x)
float cfgTempOk    = TEMP_OK_MAX;
float cfgTempWarn  = TEMP_WARN_MAX;
float cfgVoltRedLo = VOLT_RED_LOW;
float cfgVoltRedHi = VOLT_RED_HIGH;
float cfgVoltCal   = VOLT_CAL;
int   cfgBright    = 100;             // luminosité rétroéclairage 0..100 %

// ---- Simulation RPM (en attente du câblage opto 4N35) ----
uint16_t simRpm = 800;
int      rpmDir = 1;

// ---- Valeur RPM unifiée (sim ou mesurée) lue par l'aiguille, l'alarme et le web ----
uint16_t rpmValue = 800;

#if RPM_SOURCE_REAL
// ---- Capture RPM réelle : interruption sur GPIO27 + fenêtre de comptage ----
volatile uint32_t rpmPulseCount = 0;
volatile uint32_t rpmLastEdgeUs = 0;
const uint32_t RPM_DEBOUNCE_US = 1500;   // ignore 2 fronts à <1,5 ms (parasites d'allumage)
const uint32_t RPM_WINDOW_MS   = 250;    // fenêtre de comptage
uint32_t lastRpmCalc = 0;
float    rpmMeasured = 0.0f;             // RPM lissé (EMA)
const float RPM_EMA_ALPHA = 0.4f;
const float RPM_ZERO_FLOOR = 100.0f;     // sous ce RPM affiché = moteur arrêté → 0 franc (tue le résidu EMA + parasites)

void IRAM_ATTR rpmISR() {
  uint32_t t = micros();
  if (t - rpmLastEdgeUs >= RPM_DEBOUNCE_US) { rpmPulseCount++; rpmLastEdgeUs = t; }
}
#endif

// ---- État précédent (pour n'effacer/redessiner que ce qui change) ----
float prevAngle      = -9999.0f;
int   prevRpmShown   = -1;     // dernier RPM affiché au centre (aiguille)
int   prevTempDigit  = -999;   // dernier °C affiché en digital
int   prevVolt10     = -999;   // dernière tension affichée en digital
bool  prevTempFault  = false;  // état défaut précédent (pour redessiner au changement)
bool  prevVoltFault  = false;
bool  prevTempAlarm  = false;  // état alarme précédent du bloc température
bool  prevTempBlinkOn= false;  // phase de clignotement déjà rendue sur la valeur temp
bool  prevVoltAlarm  = false;  // idem pour la tension batterie
bool  prevVoltBlinkOn= false;

// ---- Min/max persistés (NVS) : récap au démarrage ----
Preferences prefs;
float tempMax = -999.0f;
float voltMin =  999.0f;
float voltMax = -999.0f;
bool  statsDirty = false;
uint32_t lastStatsSave = 0;
const uint32_t STATS_SAVE_MS = 30000;   // sauvegarde flash au plus toutes les 30 s si modifié

// ---- Alarme surchauffe : anneau rouge clignotant (r 111-116, hors aiguille/arc) ----
const uint32_t ALARM_BLINK_MS = 400;
uint32_t lastAlarmBlink = 0;
bool alarmBlinkOn    = false;
bool prevAlarmActive = false;
bool ringDrawn       = false;

uint32_t lastSim = 0, lastFrame = 0;
const uint32_t SIM_MS   = 50;
const uint32_t FRAME_MS = 40;   // 25 fps

// ---- Chien de garde (Task WDT) : reboot auto si loop() bloqué > WDT_TIMEOUT_S ----
#define WDT_TIMEOUT_S 5

// ---- Auto-réparation écran : ré-init du GC9A01 si l'image fige (faux contact) ----
// Le watchdog CPU ne voit PAS un écran figé (loop() tourne toujours). Ré-init = ~150 ms,
// sans toucher WiFi/capteurs/uptime. 0 = auto désactivé (aucun clignotement) ; ex. 15000 =
// ré-init+redraw toutes les 15 s (récupération mains libres, au prix d'un bref blink).
#define SCREEN_HEAL_MS 0
uint32_t lastScreenHeal = 0;
bool screenReinitReq = false;   // demande de ré-init écran (bouton web /screen ou auto)

// ---- Console série tamponnée pour la page web (miroir de Serial, RAM seule) ----
#define LOG_NLINES   40
#define LOG_LINELEN  64
char    logBuf[LOG_NLINES][LOG_LINELEN];
uint8_t logHead = 0, logCount = 0;
char    logCur[LOG_LINELEN];
uint8_t logCurLen = 0;

class WebLog : public Print {
public:
  size_t write(uint8_t c) override {
    Serial.write(c);                          // miroir vers le port série
    if (c == '\r') return 1;
    if (c == '\n') {                          // fin de ligne → on la range dans le ring buffer
      logCur[logCurLen] = '\0';
      memcpy(logBuf[logHead], logCur, logCurLen + 1);
      logHead = (logHead + 1) % LOG_NLINES;
      if (logCount < LOG_NLINES) logCount++;
      logCurLen = 0;
      return 1;
    }
    if (logCurLen < LOG_LINELEN - 1) logCur[logCurLen++] = (char)c;
    return 1;
  }
};
WebLog webLog;

/* ===================================================================== */

void setup() {
  Serial.begin(115200);
  // Rétroéclairage en PWM (LEDC) pour la luminosité configurable
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_TFT_BL, 5000, 8);
#else
  ledcSetup(BL_LEDC_CH, 5000, 8);
  ledcAttachPin(PIN_TFT_BL, BL_LEDC_CH);
#endif
  setBacklight(100);                  // plein éclairage au boot (avant lecture NVS)

  // ---- DS18B20 ----
  ds.begin();
  ds.setResolution(11);
  ds.setWaitForConversion(false);
  dsFound = (ds.getDeviceCount() > 0);
  if (dsFound) {
    DBG_PRINTLN(F("[DS18B20] Sonde trouvée."));
    ds.requestTemperatures();
    lastTempReq = millis();
  } else {
    DBG_PRINTLN(F("[DS18B20] SONDE INTROUVABLE — vérifier câblage et pull-up."));
  }

  // ---- ADS1115 (I2C) ----
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  adsFound = ads.begin();
  if (adsFound) {
    ads.setGain(GAIN_ONE);
    DBG_PRINTLN(F("[ADS1115] Trouvé."));
  } else {
    DBG_PRINTLN(F("[ADS1115] INTROUVABLE — vérifier câblage I2C."));
  }

#if RPM_SOURCE_REAL
  // ---- Capture RPM : entrée opto sur GPIO27, interruption sur front descendant ----
  pinMode(PIN_RPM, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_RPM), rpmISR, FALLING);
  DBG_PRINTLN(F("[RPM] Capture réelle active (GPIO27)."));
#endif

  gfx->begin(GFX_SPI_SPEED);

  // ---- Récap du trajet précédent (min/max persistés en NVS) ----
  prefs.begin("4l", false);
  loadConfig();                       // réglages persistés (seuils, calibration, luminosité)
  setBacklight(cfgBright);
  tempMax = prefs.getFloat("tMax", -999.0f);
  voltMin = prefs.getFloat("vMin",  999.0f);
  voltMax = prefs.getFloat("vMax", -999.0f);
  showStartupRecap();                       // affiche ~3 s puis rend la main
  tempMax = -999.0f; voltMin = 999.0f; voltMax = -999.0f;  // stats neuves pour ce trajet

  gfx->fillScreen(COL_BG);
  drawStaticBackground();
#if ENABLE_WEB
  setupWeb();
#endif

  // ---- Chien de garde : surveille loop(), reboot si figée > WDT_TIMEOUT_S ----
  // (abonné ici, APRÈS showStartupRecap() qui contient un delay(3000))
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtCfg = {
    .timeout_ms     = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,            // surveille seulement loop(), pas les tâches idle
    .trigger_panic  = true,         // timeout → panic → reboot
  };
  if (esp_task_wdt_init(&wdtCfg) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdtCfg);   // le core 3.x a déjà initialisé le TWDT
  }
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);  // (timeout en s, panic=true)
#endif
  esp_task_wdt_add(NULL);           // abonne la tâche courante (loopTask)
  DBG_PRINTLN(F("[WDT] Chien de garde actif."));

  DBG_PRINTLN(F("[4L] Cadran démarré."));
}

void loop() {
  uint32_t now = millis();

  esp_task_wdt_reset();             // nourrit le chien de garde

#if ENABLE_WEB
  dnsServer.processNextRequest();
  webServer.handleClient();
#endif

  // ---- DS18B20 : re-détection si absente au boot (récupération sans reboot) ----
  if (!dsFound && (now - lastDsScan >= DS_RESCAN_MS)) {
    lastDsScan = now;
    ds.begin();
    if (ds.getDeviceCount() > 0) {
      dsFound = true;
      ds.setResolution(11);
      ds.setWaitForConversion(false);
      ds.requestTemperatures();
      lastTempReq = now;
      DBG_PRINTLN(F("[DS18B20] Sonde (re)détectée."));
    }
  }

  // ---- DS18B20 non-bloquant ----
  if (dsFound && (now - lastTempReq >= TEMP_CONV_MS)) {
    float t = ds.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C && t > -50.0f && t < 150.0f) {
      realTemp = t;
      tempValid = true;
      if (realTemp > tempMax) { tempMax = realTemp; statsDirty = true; }
      DBG_PRINT(F("T=")); DBG_PRINT(realTemp, 1); DBG_PRINTLN(F(" C"));
    } else {
      tempValid = false;   // sonde déconnectée en route → bascule en défaut
    }
    ds.requestTemperatures();
    lastTempReq = now;
  }

  // ---- ADS1115 (tension batterie) ----
  if (adsFound && (now - lastVoltRead >= VOLT_READ_MS)) {
    lastVoltRead = now;
    int16_t raw = ads.readADC_SingleEnded(0);
    float vAdc  = ads.computeVolts(raw);
    float vNew  = (vAdc / DIV_RATIO) * cfgVoltCal;
    if (voltSeeded) realVolt += VOLT_EMA_ALPHA * (vNew - realVolt);
    else          { realVolt = vNew; voltSeeded = true; }
    if (realVolt < voltMin) { voltMin = realVolt; statsDirty = true; }
    if (realVolt > voltMax) { voltMax = realVolt; statsDirty = true; }
    DBG_PRINT(F("U=")); DBG_PRINT(realVolt, 2); DBG_PRINTLN(F(" V"));
  }

  // ---- RPM : simulation ou capture réelle → valeur unifiée rpmValue ----
#if RPM_SOURCE_REAL
  if (now - lastRpmCalc >= RPM_WINDOW_MS) {
    uint32_t dt = now - lastRpmCalc;
    lastRpmCalc = now;
    noInterrupts();
    uint32_t count = rpmPulseCount;
    rpmPulseCount = 0;
    interrupts();
    float inst = (float)count * 30000.0f / (float)dt;   // imp/s × 30 (2 étincelles/tour)
    inst *= RPM_TEST_MULT;                              // multiplicateur de test (=1 en usage normal)
    if (inst > 9000.0f) inst = 9000.0f;                 // garde-fou parasites
    rpmMeasured += RPM_EMA_ALPHA * (inst - rpmMeasured);
    if (rpmMeasured < RPM_ZERO_FLOOR) rpmMeasured = 0.0f;   // plancher : aucun moteur sous ~100 tr/min
    rpmValue = (uint16_t)(rpmMeasured + 0.5f);
    if (count > 0) {                                    // log brut : confirme que les fronts sont captés
      DBG_PRINT(F("[RPM] count=")); DBG_PRINT(count);
      DBG_PRINT(F(" win=")); DBG_PRINT(dt); DBG_PRINT(F("ms -> "));
      DBG_PRINT(rpmValue); DBG_PRINTLN(F(" tr/min"));
    }
  }
#else
  if (now - lastSim >= SIM_MS) {
    lastSim = now;
    updateSim();
    rpmValue = simRpm;
  }
#endif

  // ---- Sauvegarde périodique des min/max (NVS) ----
  if (statsDirty && (now - lastStatsSave >= STATS_SAVE_MS)) {
    lastStatsSave = now;
    saveStats();
    statsDirty = false;
  }

  // ---- Auto-réparation / ré-init écran (faux contact GC9A01) ----
#if SCREEN_HEAL_MS > 0
  if (now - lastScreenHeal >= SCREEN_HEAL_MS) { lastScreenHeal = now; screenReinitReq = true; }
#endif
  if (screenReinitReq) { screenReinitReq = false; reinitDisplay(); }

  // ---- Rendu ----
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    updateAlarm(now);
    updateNeedle();
    updateDigital();
  }
}

// Ré-initialise l'écran seul (GC9A01 décroché sur faux contact) sans reboot : ré-init du
// contrôleur + fond + redraw forcé de tous les éléments dynamiques (sentinelles remises à zéro).
void reinitDisplay() {
  gfx->begin(GFX_SPI_SPEED);
  gfx->fillScreen(COL_BG);
  drawStaticBackground();
  prevAngle = -9999.0f; prevRpmShown = -1;
  prevTempDigit = -999;  prevVolt10 = -999;
  prevTempFault = false; prevVoltFault = false;
  prevTempAlarm = false; prevTempBlinkOn = false;
  prevVoltAlarm = false; prevVoltBlinkOn = false;
  prevAlarmActive = false; ringDrawn = false; alarmBlinkOn = false;
  DBG_PRINTLN(F("[ECRAN] Re-init + redraw complet."));
}

/* =====================================================================
   SIMULATION RPM
   ===================================================================== */
void updateSim() {
  simRpm += rpmDir * 35;          // balayage plus large pour couvrir 0-7000
  if (simRpm >= 6800) rpmDir = -1;
  if (simRpm <= 600)  rpmDir =  1;
}

/* =====================================================================
   RÉCAP DÉMARRAGE + STATS MIN/MAX (persistées en NVS)
   ===================================================================== */
void showStartupRecap() {
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(INK); gfx->setTextSize(1);
  printCentered("TRAJET PRECEDENT", CX, 72);

  char buf[24];
  gfx->setTextSize(2); gfx->setTextColor(WARM_BROWN);

  if (tempMax > -100.0f) snprintf(buf, sizeof(buf), "Tmax %dC", (int)roundf(tempMax));
  else                   snprintf(buf, sizeof(buf), "Tmax --");
  printCentered(buf, CX, 112);

  if (voltMax > -100.0f) {
    int lo = (int)roundf(voltMin * 10.0f), hi = (int)roundf(voltMax * 10.0f);
    snprintf(buf, sizeof(buf), "%d.%d-%d.%dV", lo/10, lo%10, hi/10, hi%10);
  } else {
    snprintf(buf, sizeof(buf), "U --");
  }
  printCentered(buf, CX, 148);

  delay(10000);
}

void saveStats() {
  prefs.putFloat("tMax", tempMax);
  prefs.putFloat("vMin", voltMin);
  prefs.putFloat("vMax", voltMax);
  DBG_PRINTLN(F("[STATS] min/max sauvegardés (NVS)."));
}

/* =====================================================================
   RÉGLAGES CONFIGURABLES — luminosité + persistance NVS
   ===================================================================== */
void setBacklight(uint8_t pct) {            // 0..100 %
  if (pct > 100) pct = 100;
  uint32_t duty = (uint32_t)pct * 255 / 100;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_TFT_BL, duty);
#else
  ledcWrite(BL_LEDC_CH, duty);
#endif
}

void loadConfig() {
  cfgTempOk    = prefs.getFloat("cTempOk", TEMP_OK_MAX);
  cfgTempWarn  = prefs.getFloat("cTempWn", TEMP_WARN_MAX);
  cfgVoltRedLo = prefs.getFloat("cVLo",    VOLT_RED_LOW);
  cfgVoltRedHi = prefs.getFloat("cVHi",    VOLT_RED_HIGH);
  cfgVoltCal   = prefs.getFloat("cVCal",   VOLT_CAL);
  cfgBright    = prefs.getInt(  "cBright", 100);
}

void saveConfig() {
  prefs.putFloat("cTempOk", cfgTempOk);
  prefs.putFloat("cTempWn", cfgTempWarn);
  prefs.putFloat("cVLo",    cfgVoltRedLo);
  prefs.putFloat("cVHi",    cfgVoltRedHi);
  prefs.putFloat("cVCal",   cfgVoltCal);
  prefs.putInt(  "cBright", cfgBright);
  DBG_PRINTLN(F("[CFG] réglages sauvegardés (NVS)."));
}

/* =====================================================================
   ALARME SURCHAUFFE — anneau rouge clignotant (r 111-116, hors aiguille/arc)
   ===================================================================== */
void drawAlarmRing(uint16_t col) {
  for (int r = 111; r <= 116; r++) gfx->drawCircle(CX, CY, r, col);
}

// Condition d'alarme partagée (anneau écran + JSON web) : surchauffe, sur-régime, tension hors plage
bool alarmActiveNow() {
  return (tempValid && realTemp >= cfgTempWarn)
      || (rpmValue >= RPM_WARN_MAX)
      || (adsFound && voltSeeded && (realVolt < cfgVoltRedLo || realVolt > cfgVoltRedHi));
}

void updateAlarm(uint32_t now) {
  bool alarmActive = alarmActiveNow();

  if (!alarmActive) {
    if (ringDrawn) { drawAlarmRing(COL_BG); ringDrawn = false; }
    prevAlarmActive = false;
    return;
  }

  if (!prevAlarmActive) {                 // entrée en alarme
    alarmBlinkOn    = true;
    lastAlarmBlink  = now;
    drawAlarmRing(RED); ringDrawn = true;
    prevAlarmActive = true;
  } else if (now - lastAlarmBlink >= ALARM_BLINK_MS) {
    lastAlarmBlink = now;
    alarmBlinkOn   = !alarmBlinkOn;
    if (alarmBlinkOn) { drawAlarmRing(RED);    ringDrawn = true;  }
    else              { drawAlarmRing(COL_BG); ringDrawn = false; }
  }
}

#if ENABLE_WEB
/* =====================================================================
   INTERFACE WEB — AP WiFi + page embarquée + JSON /data (lecture seule)
   ===================================================================== */
#include "index_html.h"

void handleRoot() { webServer.send_P(200, "text/html", INDEX_HTML); }

void handleData() {
  char json[256];
  snprintf(json, sizeof(json),
    "{\"rpm\":%u,\"tempValid\":%s,\"temp\":%.1f,\"voltValid\":%s,\"volt\":%.2f,"
    "\"tMax\":%.1f,\"vMin\":%.2f,\"vMax\":%.2f,\"alarm\":%s,\"fw\":\"%s\"}",
    rpmValue, tempValid ? "true" : "false", realTemp,
    adsFound ? "true" : "false", realVolt,
    tempMax, voltMin, voltMax, alarmActiveNow() ? "true" : "false", FW_VERSION);
  webServer.send(200, "application/json", json);
}

void handleLog() {                    // GET /log → console série tamponnée (texte brut)
  static char out[LOG_NLINES * LOG_LINELEN + 16];
  size_t pos = 0;
  uint8_t idx = (logCount < LOG_NLINES) ? 0 : logHead;   // plus ancienne ligne d'abord
  for (uint8_t i = 0; i < logCount; i++) {
    size_t len = strlen(logBuf[idx]);
    if (pos + len + 1 >= sizeof(out)) break;
    memcpy(out + pos, logBuf[idx], len); pos += len;
    out[pos++] = '\n';
    idx = (idx + 1) % LOG_NLINES;
  }
  out[pos] = '\0';
  webServer.send(200, "text/plain", out);
}

void handleConfig() {                 // GET /config → réglages courants (JSON)
  char j[200];
  snprintf(j, sizeof(j),
    "{\"tempOk\":%.0f,\"tempWarn\":%.0f,\"voltLo\":%.1f,\"voltHi\":%.1f,\"voltCal\":%.3f,\"bright\":%d}",
    cfgTempOk, cfgTempWarn, cfgVoltRedLo, cfgVoltRedHi, cfgVoltCal, cfgBright);
  webServer.send(200, "application/json", j);
}

void handleSet() {                    // GET /set?...&token=..  (écriture protégée)
  if (webServer.arg("token") != CFG_TOKEN) { webServer.send(403, "text/plain", "forbidden"); return; }
  if (webServer.hasArg("tempOk"))   cfgTempOk    = webServer.arg("tempOk").toFloat();
  if (webServer.hasArg("tempWarn")) cfgTempWarn  = webServer.arg("tempWarn").toFloat();
  if (webServer.hasArg("voltLo"))   cfgVoltRedLo = webServer.arg("voltLo").toFloat();
  if (webServer.hasArg("voltHi"))   cfgVoltRedHi = webServer.arg("voltHi").toFloat();
  if (webServer.hasArg("voltCal"))  cfgVoltCal   = webServer.arg("voltCal").toFloat();
  if (webServer.hasArg("bright")) { cfgBright = webServer.arg("bright").toInt(); setBacklight(cfgBright); }
  saveConfig();
  prevTempDigit = -30000; prevVolt10 = -30000;   // force le redraw des valeurs avec les nouveaux seuils
  handleConfig();                     // renvoie la config à jour
}

void handleReset() {                  // GET /reset?token=..  (écriture protégée)
  if (webServer.arg("token") != CFG_TOKEN) { webServer.send(403, "text/plain", "forbidden"); return; }
  tempMax = -999.0f; voltMin = 999.0f; voltMax = -999.0f;
  prefs.putFloat("tMax", tempMax);
  prefs.putFloat("vMin", voltMin);
  prefs.putFloat("vMax", voltMax);
  webServer.send(200, "text/plain", "ok");
}

// --- OTA : flash du firmware via POST /update?token=.. (upload multipart) ---
static bool otaOk = false;            // mise à jour autorisée ET en cours sans erreur

void handleUpdateUpload() {
  HTTPUpload& up = webServer.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaOk = false;
    if (webServer.arg("token") != CFG_TOKEN) {         // jeton passé en query string
      DBG_PRINTLN(F("[OTA] Jeton refusé — upload ignoré."));
      return;
    }
    DBG_PRINT(F("[OTA] Début flash : ")); DBG_PRINTLN(up.filename);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); return; }
    otaOk = true;
    esp_task_wdt_reset();             // l'upload bloque loop() → on nourrit le WDT ici
  } else if (up.status == UPLOAD_FILE_WRITE && otaOk) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) { Update.printError(Serial); Update.abort(); otaOk = false; }
    esp_task_wdt_reset();
  } else if (up.status == UPLOAD_FILE_END && otaOk) {
    if (Update.end(true)) { DBG_PRINT(F("[OTA] OK, ")); DBG_PRINT(up.totalSize); DBG_PRINTLN(F(" octets écrits.")); }
    else { Update.printError(Serial); otaOk = false; }   // end() libère la session en interne
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort(); otaOk = false;
    DBG_PRINTLN(F("[OTA] Upload interrompu."));
  }
}

void handleUpdateDone() {             // appelé une fois l'upload terminé
  if (webServer.arg("token") != CFG_TOKEN) { webServer.send(403, "text/plain", "forbidden"); return; }
  bool ok = otaOk && !Update.hasError();
  webServer.send(200, "text/plain", ok ? "OK, redemarrage..." : "ECHEC (voir console serie)");
  if (ok) { delay(400); ESP.restart(); }
}

void handleReboot() {                 // GET /reboot?token=..  (redémarre l'ESP)
  if (webServer.arg("token") != CFG_TOKEN) { webServer.send(403, "text/plain", "forbidden"); return; }
  webServer.send(200, "text/plain", "Redemarrage...");
  delay(300);                         // laisse la réponse partir avant le reboot
  ESP.restart();
}

void handleScreen() {                 // GET /screen : ré-init écran seul (bénin → sans jeton)
  screenReinitReq = true;             // traité dans loop(), pas ici (cohérence du rendu)
  webServer.send(200, "text/plain", "Re-init ecran demandee");
}

void setupWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // réduit le pic de courant (anti-brownout) ; tél. à côté

  // Portail captif : toute requête DNS pointe vers l'AP → la page s'ouvre seule
  dnsServer.start(53, "*", WiFi.softAPIP());

  webServer.on("/", handleRoot);
  webServer.on("/data", handleData);
  webServer.on("/log", handleLog);
  webServer.on("/config", handleConfig);
  webServer.on("/set", handleSet);
  webServer.on("/reset", handleReset);
  webServer.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);  // OTA (jeton requis)
  webServer.on("/reboot", handleReboot);   // redémarrage ESP (jeton requis)
  webServer.on("/screen", handleScreen);   // ré-init écran seul (bénin, sans jeton)
  webServer.onNotFound(handleRoot);     // URL inconnue (sondes captives incluses) → dashboard
  webServer.begin();
  DBG_PRINT(F("[WEB] AP \"")); DBG_PRINT(AP_SSID);
  DBG_PRINT(F("\" actif — http://")); DBG_PRINT(WiFi.softAPIP());
  DBG_PRINTLN(F("/"));
}
#endif

/* =====================================================================
   FOND STATIQUE — dessiné une seule fois au setup()
   ===================================================================== */
void drawStaticBackground() {
  // Arc de couleur : 3 zones (vert / orange / rouge)
  drawArcZone(rpmToAngle(RPM_MIN),      rpmToAngle(RPM_OK_MAX),   ZONE_GREEN);
  drawArcZone(rpmToAngle(RPM_OK_MAX),   rpmToAngle(RPM_WARN_MAX), AMBER);
  drawArcZone(rpmToAngle(RPM_WARN_MAX), rpmToAngle(RPM_MAX),      RED);

  // Graduations (0 à 7, convention tachymètre = x1000 tr/min)
  for (int i = 0; i < 8; i++) drawOneGraduation(i);

  // Bord extérieur
  gfx->drawCircle(CX, CY, 118, WARM_BROWN);
  gfx->drawCircle(CX, CY, 117, WARM_BROWN);

  // Libellé "TR/MIN x1000"
  gfx->setTextColor(WARM_BROWN); gfx->setTextSize(1);
  printCentered("TR/MIN x1000", CX, CY - 35);

  // Séparateur bas
  gfx->drawFastHLine(68, 159, 104, SEPARATOR);
  gfx->drawFastHLine(68, 160, 104, SEPARATOR);

  // Libellés fixes des valeurs digitales
  gfx->setTextColor(WARM_BROWN); gfx->setTextSize(1);
  printCentered("BATTERIE", 82, 207);
  printCentered("TEMP EAU", 158, 207);
}

/* =====================================================================
   AIGUILLE RPM — mise à jour intelligente (efface seulement l'ancienne)
   ===================================================================== */
void updateNeedle() {
  float angle = rpmToAngle((float)rpmValue);

  // Ne redessine que si l'aiguille a bougé de plus de 0.5°
  if (fabsf(angle - prevAngle) < 0.5f) return;

  // 1. Efface l'ancienne aiguille
  if (prevAngle > -999.0f) {
    drawNeedleLines(prevAngle, COL_BG);
    restoreArc(prevAngle);   // restaure arc + graduations effacées
  }

  // Couleur selon la zone de régime
  uint16_t col = RUST_COL;
  if      (rpmValue >= RPM_WARN_MAX) col = RED;
  else if (rpmValue >= RPM_OK_MAX)   col = ORANGE;

  // 2. Textes AVANT l'aiguille → l'aiguille passera par-dessus (réaliste)
  prevRpmShown = rpmValue;
  gfx->fillRect(CX - 52, CY - 22, 104, 38, COL_BG);
  gfx->setTextSize(4); gfx->setTextColor(col);
  char buf[6]; snprintf(buf, sizeof(buf), "%d", rpmValue);
  printCentered(buf, CX, CY - 2);

  // "TR/MIN x1000" toujours redessiné : l'aiguille passe dessus et l'efface
  gfx->setTextSize(1); gfx->setTextColor(WARM_BROWN);
  printCentered("TR/MIN x1000", CX, CY - 35);

  // 3. Aiguille par-dessus les textes
  drawNeedleLines(angle, col);

  // 4. Pivot par-dessus tout
  gfx->fillCircle(CX, CY, 9, PIVOT_OUT);
  gfx->fillCircle(CX, CY, 5, COL_BG);

  prevAngle = angle;
}

void drawNeedleLines(float angleDeg, uint16_t col) {
  float rad  = angleDeg * DEG2RAD;
  float perpR= rad + HALF_PI_F;
  int nx = CX + (int)(NEEDLE_R   * cosf(rad));
  int ny = CY + (int)(NEEDLE_R   * sinf(rad));
  int bx = CX - (int)(NEEDLE_BACK* cosf(rad));
  int by = CY - (int)(NEEDLE_BACK* sinf(rad));
  for (int d = -2; d <= 2; d++) {
    int ox = (int)(d * cosf(perpR));
    int oy = (int)(d * sinf(perpR));
    gfx->drawLine(bx+ox, by+oy, nx+ox, ny+oy, col);
  }
}

// Dessine une graduation individuelle (trait + label) — appelable à tout moment
void drawOneGraduation(int i) {
  float rad = rpmToAngle(gradRpm[i]) * DEG2RAD;
  gfx->drawLine(CX+(int)(87*cosf(rad)),  CY+(int)(87*sinf(rad)),
                CX+(int)(109*cosf(rad)), CY+(int)(109*sinf(rad)), INK);
  gfx->drawLine(CX+(int)(88*cosf(rad)),  CY+(int)(88*sinf(rad)),
                CX+(int)(110*cosf(rad)), CY+(int)(110*sinf(rad)), INK);
  int tx = CX + (int)(77 * cosf(rad));
  int ty = CY + (int)(77 * sinf(rad));
  gfx->setTextColor(INK); gfx->setTextSize(1);
  int16_t bx, by; uint16_t bw, bh;
  gfx->getTextBounds(gradLabels[i], 0, 0, &bx, &by, &bw, &bh);
  gfx->setCursor(tx - bw/2, ty - bh/2);
  gfx->print(gradLabels[i]);
}

// Restaure l'arc + les graduations effacées par l'aiguille
void restoreArc(float angleDeg) {
  float a1 = angleDeg - 14.0f;
  float a2 = angleDeg + 14.0f;

  // 1. Restaure les pixels d'arc coloré
  for (float a = a1; a <= a2; a += 0.3f) {
    float rpm = angleToRpm(a);
    if (rpm < RPM_MIN || rpm > RPM_MAX) continue;
    uint16_t col = zoneColorForRpm(rpm);
    float rad = a * DEG2RAD;
    int yi = CY + (int)(ARC_R_INNER * sinf(rad));
    int yo = CY + (int)(ARC_R_OUTER * sinf(rad));
    if (yi > 172 || yo > 172) continue;
    gfx->drawLine(
      CX+(int)(ARC_R_INNER*cosf(rad)), yi,
      CX+(int)(ARC_R_OUTER*cosf(rad)), yo, col);
  }

  // 2. Redessine les graduations dont l'angle tombe dans la zone affectée
  for (int i = 0; i < 8; i++) {
    float ga = rpmToAngle(gradRpm[i]);
    if (ga >= angleDeg - 14.0f && ga <= angleDeg + 14.0f) {
      drawOneGraduation(i);
    }
  }
}

/* =====================================================================
   VALEURS DIGITALES — température + tension, mise à jour si ça change
   ===================================================================== */
void updateDigital() {
  // --- Tension batterie (clignote en phase avec l'anneau si hors plage) ---
  bool vFault = !adsFound;                       // défaut = ADS absent au boot
  bool vAlarm = (!vFault && voltSeeded && (realVolt < cfgVoltRedLo || realVolt > cfgVoltRedHi));
  int  v10    = (int)roundf(realVolt * 10.0f);
  bool vRedraw = (vFault != prevVoltFault)
              || (vAlarm != prevVoltAlarm)
              || (!vFault && v10 != prevVolt10)
              || (vAlarm && alarmBlinkOn != prevVoltBlinkOn);
  if (vRedraw) {
    prevVoltFault   = vFault;
    prevVoltAlarm   = vAlarm;
    prevVolt10      = v10;
    prevVoltBlinkOn = alarmBlinkOn;
    gfx->fillRect(38, 173, 97, 22, COL_BG);
    gfx->setTextSize(2);
    if (vFault) {
      gfx->setTextColor(RUST_COL);
      printCentered("--V", 82, 183);
    } else if (vAlarm && !alarmBlinkOn) {
      // phase éteinte du clignotement : case laissée vide
    } else {
      uint16_t col = DARKGREEN;
      if      (realVolt < cfgVoltRedLo || realVolt > cfgVoltRedHi) col = RED;
      else if (realVolt < 12.4f || realVolt > 14.8f)               col = ORANGE;
      gfx->setTextColor(col);
      char buf[10];
      snprintf(buf, sizeof(buf), "%d.%dV", v10/10, abs(v10%10));
      printCentered(buf, 82, 183);
    }
  }

  // --- Température liquide (clignote en phase avec l'anneau si alarme) ---
  bool tFault = !tempValid;                      // défaut = sonde absente/déconnectée
  bool tAlarm = (!tFault && realTemp >= cfgTempWarn);
  int  tInt   = (int)roundf(realTemp);
  bool tRedraw = (tFault != prevTempFault)
              || (tAlarm != prevTempAlarm)
              || (!tFault && tInt != prevTempDigit)
              || (tAlarm && alarmBlinkOn != prevTempBlinkOn);
  if (tRedraw) {
    prevTempFault   = tFault;
    prevTempAlarm   = tAlarm;
    prevTempDigit   = tInt;
    prevTempBlinkOn = alarmBlinkOn;
    gfx->fillRect(118, 173, 90, 22, COL_BG);
    gfx->setTextSize(2);
    if (tFault) {
      gfx->setTextColor(RUST_COL);
      printCentered("--C", 158, 183);
    } else if (tAlarm && !alarmBlinkOn) {
      // phase éteinte du clignotement : case laissée vide
    } else {
      uint16_t col = DARKGREEN;
      if      (realTemp >= cfgTempWarn) col = RED;
      else if (realTemp >= cfgTempOk)   col = ORANGE;
      gfx->setTextColor(col);
      char buf[8];
      snprintf(buf, sizeof(buf), "%dC", tInt);
      printCentered(buf, 158, 183);
    }
  }
}

/* =====================================================================
   UTILITAIRES
   ===================================================================== */
float rpmToAngle(float rpm) {
  return ARC_START_DEG + (rpm - RPM_MIN) / (RPM_MAX - RPM_MIN) * ARC_SPAN_DEG;
}

float angleToRpm(float angle) {
  return RPM_MIN + (angle - ARC_START_DEG) / ARC_SPAN_DEG * (RPM_MAX - RPM_MIN);
}

// Couleur de zone (vert / orange / rouge) selon le régime — source unique
uint16_t zoneColorForRpm(float rpm) {
  if      (rpm < RPM_OK_MAX)   return ZONE_GREEN;
  else if (rpm < RPM_WARN_MAX) return AMBER;
  else                         return RED;
}

void drawArcZone(float a1, float a2, uint16_t col) {
  if (a1 > a2) { float t=a1; a1=a2; a2=t; }
  for (float a = a1; a <= a2; a += 0.3f) {
    float rad = a * DEG2RAD;
    int yi = CY + (int)(ARC_R_INNER * sinf(rad));
    int yo = CY + (int)(ARC_R_OUTER * sinf(rad));
    if (yi > 172 || yo > 172) continue;
    gfx->drawLine(
      CX+(int)(ARC_R_INNER*cosf(rad)), yi,
      CX+(int)(ARC_R_OUTER*cosf(rad)), yo, col);
  }
}

void printCentered(const char *s, int x, int y) {
  int16_t bx,by; uint16_t bw,bh;
  gfx->getTextBounds(s,0,0,&bx,&by,&bw,&bh);
  gfx->setCursor(x - bw/2, y - bh/2);
  gfx->print(s);
}
