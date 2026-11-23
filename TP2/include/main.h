#ifndef MAIN_H
#define MAIN_H





#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <TM1637Display.h>

#define MATRIX_PIN 13
#define NUM_LEDS 64
#define SERVO_PIN 25
#define BUTTON_A 16
#define BUTTON_B 17
#define CLK_PIN 18
#define DIO_PIN 19

enum EtatSysteme
{
    ATTENTE,
    SAISIE_CODE,
    CLIGNOTEMENT
};

enum TypeSaisie
{
    AUCUNE_SAISIE,
    PROGRAMMATION_PREMIER_CODE,
    PROGRAMMATION_CONFIRMATION,
    OUVERTURE_COFFRE
};

enum ActionClignotement
{
    AUCUNE_ACTION,
    RECOMMENCER_PROGRAMMATION,
    PROGRAMMATION_REUSSIE,
    OUVERTURE_REFUSEE
};

extern const char *NTP_SERVER;
extern const char *TIME_ZONE;
extern const unsigned long DEBOUNCE_DELAY;
extern const unsigned long DUREE_APPUI_LONG;
extern const unsigned long DUREE_SERVO;
extern const unsigned long DUREE_CLIGNOTEMENT;

extern Adafruit_NeoPixel matrix;
extern Servo servo;
extern TM1637Display afficheur;

extern bool codeProgramme;
extern bool coffreOuvert;
extern int codeEnregistre[4];

extern EtatSysteme etatSysteme;
extern TypeSaisie typeSaisie;
extern ActionClignotement actionApresClignotement;

extern int codeTemporaire[4];
extern int codeConfirmation[4];
extern int codeSaisi[4];
extern int positionChiffre;
extern int chiffreActuel;

extern bool lectureA;
extern bool etatStableA;
extern unsigned long dernierChangementA;
extern bool appuiA;
extern bool lectureB;
extern bool etatStableB;
extern unsigned long dernierChangementB;
extern bool appuiB;
extern unsigned long tempsDebutAppuiB;
extern bool appuiLongDeclenche;

extern uint8_t couleurClignotementR;
extern uint8_t couleurClignotementG;
extern uint8_t couleurClignotementB;
extern int nombreTransitions;
extern unsigned long dernierClignotement;
extern bool couleurAllumee;

extern bool servoEnMouvement;
extern unsigned long debutMouvementServo;
extern bool wifiConnecte;
extern bool heureSynchronisee;
extern unsigned long dernierTestWiFi;
extern unsigned long dernierTestHeure;

void afficherCouleur(uint8_t r, uint8_t g, uint8_t b);
void afficherUnChiffre(int chiffre, int position);
void effacerAfficheur();

void demarrerSaisie(TypeSaisie type);
void incrementerChiffre();
void confirmerChiffre();
void terminerSaisie();

void demarrerClignotement(uint8_t r, uint8_t g, uint8_t b, ActionClignotement action);
void gererClignotement();
void terminerClignotement();

void ouvrirCoffre();
void fermerCoffre();
void programmerNouveauCode();

void gererBoutonA();
void gererBoutonB();
void lireBoutons();

void gererServo();

void connecterWiFi();
void synchroniserHeure();
void gererWiFiEtHeure();

void initialiserFirebase();
void journaliserEvenement(const char *type);
void traiterFileEvenements();

#endif
