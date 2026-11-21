
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <TM1637Display.h>

// ==========================================
// CONFIGURATION DES BROCHES
// ==========================================

#define MATRIX_PIN 13
#define NUM_LEDS 64

#define SERVO_PIN 25

#define BUTTON_A 16
#define BUTTON_B 17

#define CLK_PIN 18
#define DIO_PIN 19

// ==========================================
// CONFIGURATION WIFI
// ==========================================

const char *WIFI_SSID = "iPhone de sami";
const char *WIFI_PASSWORD = "sami2222";

// ==========================================
// CONFIGURATION NTP
// ==========================================

const char *NTP_SERVER = "pool.ntp.org";
const char *TIME_ZONE = "EST5EDT,M3.2.0/2,M11.1.0/2";

// ==========================================
// CONFIGURATION TEMPS
// ==========================================

const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long DUREE_APPUI_LONG = 3000;
const unsigned long DUREE_SERVO = 1000;

// ==========================================
// OBJETS
// ==========================================

Adafruit_NeoPixel matrix(
    NUM_LEDS,
    MATRIX_PIN,
    NEO_GRB + NEO_KHZ800);

Servo servo;

TM1637Display afficheur(CLK_PIN, DIO_PIN);

// ==========================================
// VARIABLES
// ==========================================

// État du coffre
bool codeProgramme = false;
bool coffreOuvert = true;

// Code enregistré
int codeEnregistre[4] = {0, 0, 0, 0};

// ==========================================
// DÉTECTION BOUTON A
// ==========================================

bool dernierEtatA = HIGH;
bool etatStableA = HIGH;

unsigned long dernierChangementA = 0;

// ==========================================
// DÉTECTION BOUTON B
// ==========================================

bool dernierEtatB = HIGH;
bool etatStableB = HIGH;

unsigned long dernierChangementB = 0;

unsigned long tempsDebutAppui = 0;

bool appuiLongDeclenche = false;

// ==========================================
// MATRICE
// ==========================================

void afficherCouleur(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        matrix.setPixelColor(i, matrix.Color(r, g, b));
    }

    matrix.show();
}

// ==========================================
// CLIGNOTEMENT DE LA MATRICE
// ==========================================

void clignoterCouleur(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < 3; i++)
    {
        afficherCouleur(r, g, b);
        delay(300);

        afficherCouleur(0, 0, 0);
        delay(300);
    }
}

// ==========================================
// CONNEXION WIFI
// ==========================================

void connecterWiFi()
{
    Serial.println("Connexion au Wi-Fi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }

    Serial.println();
    Serial.println("Wi-Fi connecte !");

    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP());
}

// ==========================================
// SYNCHRONISATION DE L'HEURE
// ==========================================

void synchroniserHeure()
{
    Serial.println("Synchronisation NTP...");

    configTzTime(TIME_ZONE, NTP_SERVER);

    struct tm timeInfo;

    if (getLocalTime(&timeInfo, 10000))
    {
        Serial.println("Heure synchronisee !");

        Serial.print("Date et heure : ");
        Serial.println(&timeInfo, "%Y-%m-%d %H:%M:%S");
    }
    else
    {
        Serial.println("Erreur de synchronisation NTP");
    }
}

// ==========================================
// ATTENDRE UN APPUI COMPLET
// ==========================================


//***************** MÉMO ************************
// HIGH  = Relaché ///  LOW = Enfoncé ***********
//***********************************************
void attendreRelachement(int bouton)
{
    while (digitalRead(bouton) == LOW)
    {
        delay(1);
    }

    delay(DEBOUNCE_DELAY);
}

void attendreAppui(int bouton)
{

    //  Boucle infini qui gère l'appui du boutton A 
    // En résumé on attend que le boutton soit enfoncé 
    while (true)
    {
        // Attendre que le bouton soit enfoncé
        while (digitalRead(bouton) == HIGH)
        {
            delay(1);
        }

        // Vérifier l'anti-rebond
        delay(DEBOUNCE_DELAY);

        if (digitalRead(bouton) == LOW)
        {
            break;
        }
    }

    // Attendre le relâchement
    attendreRelachement(bouton);
}

// ==========================================
// AFFICHER UN SEUL CHIFFRE
// ==========================================
void afficherUnChiffre(int chiffre, int position)
{
    // Modifier uniquement le digit sélectionné
    afficheur.showNumberDec(chiffre, false, 1, position);
}

// ==========================================
// SAISIE DU PREMIER CODE
// ==========================================

void changerCode(int code[4])
{
    Serial.println("==============================");
    Serial.println("PREMIERE SAISIE");
    Serial.println("==============================");

    // Attendre le relâchement du B ayant déclenché
    // la programmation
    attendreRelachement(BUTTON_B);

    afficheur.clear();
    for (int i = 0; i < 4; i++)
    {
        int currentNumber = 0;

        bool codeConfirmed = false;

        // Afficher le premier chiffre
        afficherUnChiffre(currentNumber, i);

        while (!codeConfirmed)
        {
            // ==========================================
            // BOUTON A : INCRÉMENTER LE CHIFFRE
            // ==========================================

            if (digitalRead(BUTTON_A) == LOW)
            {
                attendreAppui(BUTTON_A);

                currentNumber = (currentNumber + 1) % 10;

                afficherUnChiffre(currentNumber, i);

                Serial.println("Chiffre modifie");
            }

            // ==========================================
            // BOUTON B : CONFIRMER LE CHIFFRE
            // ==========================================

            if (digitalRead(BUTTON_B) == LOW)
            {
                attendreAppui(BUTTON_B);

                code[i] = currentNumber;

                codeConfirmed = true;

                Serial.print("Position confirmee : ");
                Serial.println(i + 1);
            }
        }
    }

    // Effacer l'afficheur après la première saisie
    afficheur.clear();

    Serial.println("Premiere saisie terminee");
}

// ==========================================
// CONFIRMATION DU CODE
// ==========================================

bool confirmerCode(const int code[4])
{
    int confirmedCode[4] = {0, 0, 0, 0};

    Serial.println("==============================");
    Serial.println("CONFIRMATION DU CODE");
    Serial.println("==============================");

    for (int i = 0; i < 4; i++)
    {
        int currentNumber = 0;

        bool codeConfirmed = false;

        afficherUnChiffre(currentNumber, i);

        while (!codeConfirmed)
        {
            // ==========================================
            // BOUTON A : INCRÉMENTER LE CHIFFRE
            // ==========================================

            if (digitalRead(BUTTON_A) == LOW)
            {
                attendreAppui(BUTTON_A);

                currentNumber = (currentNumber + 1) % 10;

                afficherUnChiffre(currentNumber, i);

                Serial.println("Chiffre modifie");
            }

            // ==========================================
            // BOUTON B : CONFIRMER LE CHIFFRE
            // ==========================================

            if (digitalRead(BUTTON_B) == LOW)
            {
                attendreAppui(BUTTON_B);

                confirmedCode[i] = currentNumber;

                codeConfirmed = true;

                Serial.print("Position confirmee : ");
                Serial.println(i + 1);
            }
        }
    }

    // Effacer l'afficheur
    afficheur.clear();

    Serial.println("Confirmation terminee");

    // ==========================================
    // COMPARAISON DES DEUX CODES
    // ==========================================

    for (int i = 0; i < 4; i++)
    {
        if (confirmedCode[i] != code[i])
        {
            Serial.println("Les codes sont differents !");
            return false;
        }
    }

    Serial.println("Les codes sont identiques !");

    return true;
}
// ==========================================
// FERMETURE DU COFFRE
// ==========================================

void fermerCoffre()
{
    // Vérifier que le coffre est ouvert
    // et qu'un code a été programmé

    if (!codeProgramme || !coffreOuvert)
    {
        return;
    }

    Serial.println("==============================");
    Serial.println("FERMETURE DU COFFRE");
    Serial.println("==============================");

    // Faire tourner le servo à 180 degrés
    servo.attach(SERVO_PIN, 500, 2400);
    servo.write(180);

    delay(DUREE_SERVO);

    servo.detach();

    // Matrice rouge
    afficherCouleur(255, 0, 0);

    // Effacer l'afficheur
    afficheur.clear();

    // Mettre à jour l'état du coffre
    coffreOuvert = false;

    Serial.println("Coffre ferme !");
    Serial.println("Servo : 180 degres");
    Serial.println("Matrice : rouge");
    Serial.println("Afficheur : vide");

    Serial.println("==============================");
}

// ==========================================
// PROGRAMMATION COMPLÈTE DU CODE
// ==========================================

void programmerNouveauCode()
{
    int nouveauCode[4] = {0, 0, 0, 0};

    bool programmationTerminee = false;

    while (!programmationTerminee)
    {
        // Matrice orange
        afficherCouleur(255, 80, 0);

        // Première saisie
        changerCode(nouveauCode);

        // Garder la matrice orange pendant la confirmation
        afficherCouleur(255, 80, 0);

        // Deuxième saisie et comparaison
        bool verdict = confirmerCode(nouveauCode);

        if (verdict)
        {
            // Enregistrer le nouveau code
            for (int i = 0; i < 4; i++)
            {
                codeEnregistre[i] = nouveauCode[i];
            }

            codeProgramme = true;

            Serial.println("==============================");
            Serial.println("PROGRAMMATION REUSSIE");
            Serial.println("==============================");

            // Trois clignotements verts
            clignoterCouleur(0, 255, 0);

            // Coffre toujours ouvert
            afficherCouleur(0, 255, 0);

            programmationTerminee = true;
        }
        else
        {
            Serial.println("==============================");
            Serial.println("ERREUR DE CONFIRMATION");
            Serial.println("RECOMMENCER LA SAISIE");
            Serial.println("==============================");

            // Trois clignotements rouges
            clignoterCouleur(255, 0, 0);

            // La matrice repasse en orange
            // au début de la prochaine tentative
        }
    }
}

// ==========================================
// GESTION DU BOUTON A
// ==========================================
void gererBoutonA()
{
    bool lecture = digitalRead(BUTTON_A);

    if (lecture != dernierEtatA)
    {
        dernierChangementA = millis();
    }

    if (millis() - dernierChangementA >= DEBOUNCE_DELAY)
    {
        if (lecture != etatStableA)
        {
            etatStableA = lecture;

            // Bouton A enfoncé
            if (etatStableA == LOW)
            {
                Serial.println("Bouton A enfonce");

                // Fermer uniquement si un code existe
                // et si le coffre est ouvert

                if (codeProgramme && coffreOuvert)
                {
                    fermerCoffre();
                }
                else if (!codeProgramme)
                {
                    Serial.println("Aucun code programme !");
                    Serial.println("Impossible de fermer le coffre.");
                }
                else
                {
                    Serial.println("Le coffre est deja ferme.");
                }
            }
        }
    }

    dernierEtatA = lecture;
}
// ==========================================
// GESTION DU BOUTON B
// ==========================================
void gererBoutonB()
{
    bool lecture = digitalRead(BUTTON_B);

    if (lecture != dernierEtatB)
    {
        dernierChangementB = millis();
    }

    if (millis() - dernierChangementB >= DEBOUNCE_DELAY)
    {
        if (lecture != etatStableB)
        {
            etatStableB = lecture;

            // Bouton enfoncé
            if (etatStableB == LOW)
            {
                tempsDebutAppui = millis();

                appuiLongDeclenche = false;

                Serial.println("Bouton B enfonce");
            }

            // Bouton relâché
            else
            {
                if (!appuiLongDeclenche)
                {
                    Serial.println("Appui court sur B");
                }

                Serial.println("Bouton B relache");
            }
        }
    }

    dernierEtatB = lecture;

    // ==========================================
    // DÉTECTION APPUI LONG
    // ==========================================

    if (etatStableB == LOW && !appuiLongDeclenche)
    {
        if (millis() - tempsDebutAppui >= DUREE_APPUI_LONG)
        {
            appuiLongDeclenche = true;

            Serial.println("==============================");
            Serial.println("APPUI LONG DETECTE !");
            Serial.println("==============================");

            // Programmer uniquement si le coffre est ouvert
            // et qu'aucune programmation n'est en cours
            if (coffreOuvert)
            {
                programmerNouveauCode();
            }

            // Réinitialiser l'état de lecture après la saisie
            dernierEtatB = digitalRead(BUTTON_B);
            etatStableB = dernierEtatB;
            dernierChangementB = millis();
        }
    }
}

// ==========================================
// INITIALISATION
// ==========================================

void setup()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println("==============================");
    Serial.println(" COFFRE-FORT CONNECTE");
    Serial.println(" DEMARRAGE");
    Serial.println("==============================");

    // Configuration des boutons
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);

    // Initialisation matrice
    matrix.begin();
    matrix.setBrightness(30);
    matrix.clear();

    // Initialisation afficheur
    afficheur.setBrightness(7, true);
    afficheur.clear();

    // Test afficheur
    afficheur.showNumberDec(8888, true);

    Serial.println("Test afficheur : 8888");

    // Servo à 0 degré
    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN, 500, 2400);
    servo.write(0);

    delay(1000);

    servo.detach();

    // Matrice verte
    afficherCouleur(0, 255, 0);

    codeProgramme = false;
    coffreOuvert = true;

    Serial.println("Coffre ouvert");
    Serial.println("Servo : 0 degres");
    Serial.println("Matrice : verte");
    Serial.println("Aucun code programme");

    // Connexion Wi-Fi
    connecterWiFi();

    // Synchronisation de l'heure
    synchroniserHeure();

    // Effacer le test 8888
    afficheur.clear();

    Serial.println("==============================");
    Serial.println("INITIALISATION TERMINEE");
    Serial.println("==============================");
}

// ==========================================
// BOUCLE PRINCIPALE
// ==========================================

void loop()
{
    gererBoutonB();
    gererBoutonA();
}
