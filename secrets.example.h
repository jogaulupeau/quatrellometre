#pragma once
// =====================================================================
//  MODELE de secrets.h — NE CONTIENT AUCUN VRAI SECRET (partageable).
//
//  Usage : copie ce fichier en "secrets.h" (meme dossier) et remplace
//  les placeholders ci-dessous par tes vraies valeurs.
//  "secrets.h" reste LOCAL (non partage ; a mettre au .gitignore si versionne).
//  Un "secrets.h" absent => la compilation echoue : c'est voulu, les secrets
//  ne voyagent pas avec le code source.
// =====================================================================

#define AP_PASS_SECRET   "CHANGE_MOI_8car_min"   // mot de passe WiFi de l'AP (WPA2, >= 8 caracteres)
#define CFG_TOKEN_SECRET "CHANGE_MOI_jeton"      // jeton des ecritures de config + OTA
