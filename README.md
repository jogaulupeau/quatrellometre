# Quatrellomètre

> *Quatrelle + -mètre* — l'instrument de bord de la 4L.

Cadran de télémétrie rétro pour **Renault 4L (1985)** : compte-tours à aiguille,
température du liquide de refroidissement et tension batterie sur un écran rond,
piloté par un **ESP32**. Une interface web déportée (point d'accès WiFi) permet de visualiser
les mesures, configurer les seuils et mettre à jour le firmware depuis un smartphone.

> Projet personnel / embarqué. Le firmware est en C++ (framework Arduino).

---

## Fonctionnalités

- **Compte-tours à aiguille** (0–7000 tr/min) sur écran rond, avec rendu
  différentiel anti-scintillement (pas de framebuffer).
- **Température d'eau** (DS18B20) et **tension batterie** (ADS1115) en affichage
  numérique avec code couleur (vert / orange / rouge).
- **Détection de défaut capteur** : une sonde absente affiche `--` au lieu d'une
  valeur figée trompeuse.
- **Alertes visuelles** : anneau rouge clignotant mutualisé (surchauffe,
  sur-régime, tension hors plage).
- **Min/max persistés en NVS** + récapitulatif du trajet précédent au démarrage.
- **Interface web déportée** (point d'accès WiFi + portail captif) :
  - dashboard live + **cadran à aiguille SVG** (rendu côté navigateur) ;
  - **configuration** des seuils, de la calibration tension et de la luminosité,
    persistée en NVS ;
  - **console série** en direct, **mise à jour OTA** du firmware, **redémarrage**
    et **ré-initialisation de l'écran** à distance.
- **Fiabilité** : chien de garde (watchdog) qui reboote en cas de blocage,
  filtrage EMA de la tension, auto-réparation de l'écran.

---

## Matériel

| Composant | Rôle | Interface |
|-----------|------|-----------|
| ESP32-WROOM-32 | microcontrôleur (WiFi) | — |
| GC9A01 240×240 (rond, IPS) | écran | SPI |
| DS18B20 | température liquide | OneWire |
| ADS1115 | tension batterie (ADC 16 bits) | I²C (0x48) |
| Optocoupleur 4N35 | capture du régime sur la bobine | GPIO (entrée) |
| LM2596HV | abaisseur 12 V → 5 V | alimentation |

### Brochage (ESP32)

| Signal | GPIO | Remarque |
|--------|------|----------|
| TFT SCK | 18 | SPI écran |
| TFT MOSI | 23 | SPI écran |
| TFT MISO | 19 | SPI écran |
| TFT CS | 5 | |
| TFT DC | 16 | |
| TFT RST | 17 | |
| TFT BL | 4 | rétroéclairage (PWM) |
| DS18B20 DATA | 25 | pull-up 4,7 kΩ vers 3V3 |
| I²C SDA | 21 | ADS1115 |
| I²C SCL | 22 | ADS1115 |
| RPM (opto) | 27 | `INPUT_PULLUP`, actif bas |

### Mesure de tension

Pont diviseur **R1 = 100 kΩ / R2 = 18 kΩ** (rapport ≈ 0,1525) vers l'entrée de
l'ADS1115 (gain ±4,096 V). Coefficient de calibration logiciel (`VOLT_CAL`)
ajustable, ainsi que depuis l'interface web.

### Capture du régime

Optocoupleur **4N35** lisant la **borne – de la bobine** (« Cas B »). Pour un
4 cylindres / 4 temps = **2 étincelles par tour** → `RPM = impulsions/s × 30`.
Capture par interruption (front descendant) + fenêtre de comptage 250 ms + lissage.

> ⚠️ La résistance d'entrée de la LED de l'opto doit être dimensionnée pour la
> tension réelle de la bobine (pics de flyback) — voir le code et les
> commentaires associés.

---

## Architecture logicielle

- **Rendu différentiel** : l'arc, les graduations et le fond ne sont dessinés
  qu'une fois ; à chaque image, seule l'ancienne aiguille est effacée puis
  l'arc/les graduations sont restaurés localement. Pas de framebuffer (l'ESP32
  n'a pas de PSRAM). Technique sensible — à ne pas régresser.
- **Capteurs non bloquants** : conversions DS18B20 et lectures ADS1115 étalées
  dans la boucle.
- **Interface web** : serveur HTTP synchrone (`WebServer`), page embarquée en
  PROGMEM (`index_html.h`), interrogée en JSON via `/data` (polling 250 ms). Le
  cadran SVG est dessiné **côté navigateur** → coût nul pour l'ESP.
- **Persistance** : `Preferences` (NVS, espace `4l`) pour les min/max et la
  configuration.
- **Fiabilité** : `esp_task_wdt` (watchdog tâche), ré-init écran (`/screen`),
  filtre EMA tension, anneau d'alerte mutualisé.

### Principaux fichiers

| Fichier | Contenu |
|---------|---------|
| `demo_cadran_4l_1.ino` | firmware principal |
| `index_html.h` | page web embarquée (chaîne brute, hors préprocesseur de croquis) |
| `secrets.example.h` | modèle de `secrets.h` (à copier et renseigner) |

---

## Compilation & flash

1. **Arduino IDE** avec le support **ESP32** installé (Boards Manager).
2. Bibliothèques requises :
   - `Arduino_GFX_Library` (moononournation)
   - `OneWire`, `DallasTemperature`
   - `Adafruit_ADS1X15`
   - (`WiFi`, `WebServer`, `DNSServer`, `Update`, `Preferences` fournis par le core ESP32)
3. **Secrets** : copier le modèle puis renseigner ses valeurs :
   ```sh
   cp secrets.example.h secrets.h
   ```
   `secrets.h` est volontairement **hors dépôt** (voir `.gitignore`).
4. **Schéma de partition AVEC OTA** (Outils → Partition Scheme), p. ex.
   « Default 4MB with spiffs » ou « Minimal SPIFFS (1.9MB APP with OTA) ».
   ❌ Pas « Huge APP (No OTA) » sinon l'OTA est impossible.
5. **Premier flash en USB** (les mises à jour suivantes pourront se faire par OTA).

---

## Configuration (drapeaux en tête du `.ino`)

| Drapeau | Rôle |
|---------|------|
| `DEBUG` | logs série (et console web) |
| `ENABLE_WEB` | active le WiFi + l'interface web |
| `FW_VERSION` | marqueur de version affiché dans l'en-tête web |
| `RPM_SOURCE_REAL` | 0 = régime simulé, 1 = capture réelle (opto GPIO27) |
| `RPM_TEST_MULT` | multiplicateur de test au doigt (**remettre à 1** en usage réel) |
| `GFX_SPI_SPEED` | horloge SPI écran (basse = fiabilité sur câblage non blindé) |
| `SCREEN_HEAL_MS` | auto-réparation écran périodique (0 = désactivée) |
| `WDT_TIMEOUT_S` | délai du chien de garde |

Les seuils (température, tension), la calibration et la luminosité sont aussi
réglables **à chaud depuis l'interface web** et persistés en NVS.

---

## Interface web

1. Se connecter au point d'accès WiFi **`4L-Telemetrie`** (mot de passe défini
   dans `secrets.h`).
2. Le **portail captif** ouvre la page automatiquement (sinon `http://192.168.4.1`).
3. La page offre : dashboard live, cadran à aiguille, configuration, console
   série, mise à jour OTA, redémarrage et ré-init écran.

### Sécurité

- Les **écritures** (configuration, OTA, redémarrage) sont protégées par un
  **jeton** (`CFG_TOKEN`), saisi dans le champ « mot de passe » de la page.
  Modèle à deux facteurs : WPA2 pour atteindre le point d'accès + jeton pour écrire.
- Les **secrets** (mot de passe WiFi, jeton) vivent dans `secrets.h`, **hors
  dépôt**. Un modèle `secrets.example.h` documente les valeurs attendues.

---

## État

- Firmware complet et validé sur matériel (mesures réelles température / tension,
  interface web, OTA, watchdog).
- Capture du régime **validée au banc** via l'opto ; intégration sur la bobine
  réelle et étage d'alimentation protégé (anti-parasites automobiles) en cours.
- Cible matérielle finale : passage du prototype sur Veroboard.
