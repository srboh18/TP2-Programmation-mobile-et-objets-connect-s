
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

// ==========================================
// CONFIGURATION DES BROCHES
// ==========================================

#define MATRIX_PIN 13
#define NUM_LEDS 64

#define SERVO_PIN 25

#define BUTTON_A 16
#define BUTTON_B 17

// ==========================================
// CONFIGURATION WIFI
// ==========================================

const char *WIFI_SSID = "iPhone de sami";
const char *WIFI_PASSWORD = "sami2222";

// ==========================================
// CONFIGURATION NTP
// ==========================================

// SRC : Documentation officielle Espressif
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html

const char *NTP_SERVER = "pool.ntp.org";

// Fuseau horaire de Quebec
const char *TIME_ZONE = "EST5EDT,M3.2.0/2,M11.1.0/2";

// ==========================================
// CONFIGURATION APPUI LONG
// ==========================================

const unsigned long DUREE_APPUI_LONG = 3000;
const unsigned long DEBOUNCE_DELAY = 50;

// ==========================================
// OBJETS
// ==========================================

Adafruit_NeoPixel matrix(
    NUM_LEDS,
    MATRIX_PIN,
    NEO_GRB + NEO_KHZ800);

Servo servo;

// ==========================================
// VARIABLES
// ==========================================

// État du coffre
bool codeProgramme = false;
bool coffreOuvert = true;

// Détection bouton B
bool dernierEtatBouton = HIGH;
bool etatStableBouton = HIGH;

unsigned long dernierChangement = 0;
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
// GESTION DU BOUTON B
// ==========================================

void gererBoutonB()
{

    bool lecture = digitalRead(BUTTON_B);

    // Détection d'un changement d'état
    if (lecture != dernierEtatBouton)
    {
        dernierChangement = millis();
    }

    // Anti-rebond
    if (millis() - dernierChangement >= DEBOUNCE_DELAY)
    {

        if (lecture != etatStableBouton)
        {

            etatStableBouton = lecture;

            // Bouton enfoncé
            if (etatStableBouton == LOW)
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

    dernierEtatBouton = lecture;

    // Détection de l'appui long
    if (etatStableBouton == LOW && !appuiLongDeclenche)
    {

        if (millis() - tempsDebutAppui >= DUREE_APPUI_LONG)
        {

            appuiLongDeclenche = true;

            Serial.println("==============================");
            Serial.println("APPUI LONG DETECTE !");
            Serial.println("3 secondes atteintes");
            Serial.println("==============================");

            // Pour le moment, aucune programmation
            // du code n'est encore implémentée.

            afficherCouleur(255, 80, 0);
            changerCode()
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

    // Initialisation de la matrice
    matrix.begin();
    matrix.setBrightness(30);
    matrix.clear();

    // Coffre ouvert : servo à 0 degré
    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN, 500, 2400);
    servo.write(0);

    delay(1000);

    // Arrêter le signal PWM après le déplacement
    servo.detach();

    // Matrice verte : coffre ouvert
    afficherCouleur(0, 255, 0);

    // Aucun code programmé
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
}
