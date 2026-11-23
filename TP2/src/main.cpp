#include "main.h"
#include "secret.h"
#include <WiFi.h>
#include <time.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ==========================================
// CONFIGURATION NTP
// ==========================================

const char *NTP_SERVER = "pool.ntp.org";
const char *TIME_ZONE = "EST5EDT,M3.2.0/2,M11.1.0/2";

const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long DUREE_APPUI_LONG = 3000;
const unsigned long DUREE_SERVO = 1000;
const unsigned long DUREE_CLIGNOTEMENT = 300;

// ==========================================
// OBJETS
// ==========================================

Adafruit_NeoPixel matrix(NUM_LEDS, MATRIX_PIN, NEO_GRB + NEO_KHZ800);
Servo servo;
TM1637Display afficheur(CLK_PIN, DIO_PIN);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool firebaseInitialise = false;
bool firebaseInscriptionReussie = false;

// ==========================================
// ÉTATS DU COFFRE
// ==========================================

bool codeProgramme = false;
bool coffreOuvert = true;
int codeEnregistre[4] = {0, 0, 0, 0};

// ==========================================
// MACHINE À ÉTATS
// ==========================================

EtatSysteme etatSysteme = ATTENTE;
TypeSaisie typeSaisie = AUCUNE_SAISIE;
ActionClignotement actionApresClignotement = AUCUNE_ACTION;

// ==========================================
// VARIABLES DE SAISIE
// ==========================================

int codeTemporaire[4] = {0, 0, 0, 0};
int codeConfirmation[4] = {0, 0, 0, 0};
int codeSaisi[4] = {0, 0, 0, 0};
int positionChiffre = 0;
int chiffreActuel = 0;

// ==========================================
// VARIABLES ANTI-REBOND
// ==========================================

bool lectureA = HIGH;
bool etatStableA = HIGH;
unsigned long dernierChangementA = 0;
bool appuiA = false;

bool lectureB = HIGH;
bool etatStableB = HIGH;
unsigned long dernierChangementB = 0;
bool appuiB = false;
unsigned long tempsDebutAppuiB = 0;
bool appuiLongDeclenche = false;

// ==========================================
// VARIABLES CLIGNOTEMENT
// ==========================================

uint8_t couleurClignotementR = 0;
uint8_t couleurClignotementG = 0;
uint8_t couleurClignotementB = 0;
int nombreTransitions = 0;
unsigned long dernierClignotement = 0;
bool couleurAllumee = false;

// ==========================================
// VARIABLES SERVO
// ==========================================

bool servoEnMouvement = false;
unsigned long debutMouvementServo = 0;

// ==========================================
// VARIABLES WIFI ET NTP
// ==========================================

bool wifiConnecte = false;
bool heureSynchronisee = false;
unsigned long dernierTestWiFi = 0;
unsigned long dernierTestHeure = 0;

// ==========================================
// VARIABLES FIRESTORE
// ==========================================

// Compteur pour garantir un identifiant différent à chaque document Firestore.
unsigned long numeroEvenement = 0;

void afficherCouleur(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        matrix.setPixelColor(i, matrix.Color(r, g, b));
    }

    matrix.show();
}

void afficherUnChiffre(int chiffre, int position)
{
    afficheur.showNumberDec(chiffre, false, 1, position);
}

void effacerAfficheur()
{
    afficheur.clear();
}

void connecterWiFi()
{
    Serial.println("Demarrage de la connexion Wi-Fi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void synchroniserHeure()
{
    Serial.println("Demande de synchronisation NTP...");
    configTzTime(TIME_ZONE, NTP_SERVER);
}

// Initialise Firebase avec l'authentification anonyme vue dans l'exercice du cours.
void initialiserFirebase()
{
    Serial.println("[Firebase] Initialisation");

    config.api_key = API_KEY;
    config.token_status_callback = tokenStatusCallback;

    if (Firebase.signUp(&config, &auth, "", ""))
    {
        firebaseInscriptionReussie = true;
        Serial.println("Connexion Firebase OK");
    }
    else
    {
        Serial.print("Erreur Firebase : ");
        Serial.println(config.signer.signupError.message.c_str());
    }

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    firebaseInitialise = true;
}

// Enregistre un événement dans Firestore à l'aide de FirebaseJson et createDocument.
void journaliserEvenement(const char *type)
{
    time_t maintenant = time(nullptr);

    struct tm heureUTC;
    gmtime_r(&maintenant, &heureUTC);


    // Src horodotage : https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html
    char horodatage[25];
    strftime(horodatage, sizeof(horodatage), "%Y-%m-%dT%H:%M:%SZ", &heureUTC);

    FirebaseJson document;
    document.set("fields/type/stringValue", type);
    document.set("fields/horodatage/timestampValue", horodatage);
    document.set("fields/coffre/stringValue", ID_COFFRE);

    numeroEvenement++;

    String cheminDocument = "evenements/";
    cheminDocument += String((unsigned long)maintenant);
    cheminDocument += "_";
    cheminDocument += String(numeroEvenement);

    Serial.print("Envoi Firestore : ");
    Serial.println(type);


    // If pour vérifier si success du fireStore
    if (Firebase.Firestore.createDocument(
            &fbdo,
            PROJECT_ID,
            "",
            cheminDocument.c_str(),
            document.raw()))
    {
        Serial.println("Evenement ajoute a Firestore.");
    }
    else
    {
        Serial.print("Erreur Firestore : ");
        Serial.println(fbdo.errorReason());
    }
}

void gererWiFiEtHeure()
{
    if (!wifiConnecte && WiFi.status() == WL_CONNECTED)
    {
        wifiConnecte = true;

        Serial.println("Wi-Fi connecte !");
        Serial.print("Adresse IP : ");
        Serial.println(WiFi.localIP());

        synchroniserHeure();
        initialiserFirebase();
    }

    if (wifiConnecte && !heureSynchronisee)
    {
        if (millis() - dernierTestHeure >= 1000)
        {
            dernierTestHeure = millis();

            struct tm timeInfo;
            if (getLocalTime(&timeInfo, 0))
            {
                heureSynchronisee = true;
                Serial.println("Heure synchronisee !");
            }
        }
    }

    if (!wifiConnecte && millis() - dernierTestWiFi >= 5000)
    {
        dernierTestWiFi = millis();
        Serial.println("En attente du Wi-Fi...");
    }
}

// Démarre la saisie du code ou passe à l'étape de confirmation.
void demarrerSaisie(TypeSaisie type)
{
    // Autorise le passage de la première saisie à la confirmation sans quitter SAISIE_CODE.
    bool transitionConfirmation =
        etatSysteme == SAISIE_CODE &&
        typeSaisie == PROGRAMMATION_PREMIER_CODE &&
        type == PROGRAMMATION_CONFIRMATION;


    if (etatSysteme != ATTENTE && !transitionConfirmation)
    {
        return;
    }

    typeSaisie = type;
    positionChiffre = 0;
    chiffreActuel = 0;
    etatSysteme = SAISIE_CODE;

    effacerAfficheur();
    afficherUnChiffre(chiffreActuel, positionChiffre);

    if (type == PROGRAMMATION_PREMIER_CODE)
    {
        Serial.println("PREMIERE SAISIE DU CODE");
    }
    else if (type == PROGRAMMATION_CONFIRMATION)
    {
        Serial.println("CONFIRMATION DU CODE");
    }
    else if (type == OUVERTURE_COFFRE)
    {
        Serial.println("SAISIE DU CODE POUR OUVRIR");
    }
}

void incrementerChiffre()
{
    if (etatSysteme != SAISIE_CODE)
    {
        return;
    }

    chiffreActuel = (chiffreActuel + 1) % 10;
    afficherUnChiffre(chiffreActuel, positionChiffre);
}

void confirmerChiffre()
{
    if (etatSysteme != SAISIE_CODE)
    {
        return;
    }

    if (typeSaisie == PROGRAMMATION_PREMIER_CODE)
    {
        codeTemporaire[positionChiffre] = chiffreActuel;
    }
    else if (typeSaisie == PROGRAMMATION_CONFIRMATION)
    {
        codeConfirmation[positionChiffre] = chiffreActuel;
    }
    else if (typeSaisie == OUVERTURE_COFFRE)
    {
        codeSaisi[positionChiffre] = chiffreActuel;
    }

    positionChiffre++;

    if (positionChiffre >= 4)
    {
        terminerSaisie();
        return;
    }

    chiffreActuel = 0;
    afficherUnChiffre(chiffreActuel, positionChiffre);
}

// Vérifie les quatre chiffres saisis et déclenche l'action correspondante.
void terminerSaisie()
{
    effacerAfficheur();

    if (typeSaisie == PROGRAMMATION_PREMIER_CODE)
    {
        // On démare une saisie d'un code
        demarrerSaisie(PROGRAMMATION_CONFIRMATION);
        return;
    }

    if (typeSaisie == PROGRAMMATION_CONFIRMATION)
    {
        bool identiques = true;

        for (int i = 0; i < 4; i++)
        {
            if (codeTemporaire[i] != codeConfirmation[i])
            {
                identiques = false;
                break;
            }
        }

        typeSaisie = AUCUNE_SAISIE;
        etatSysteme = ATTENTE;

        if (identiques)
        {
            for (int i = 0; i < 4; i++)
            {
                codeEnregistre[i] = codeTemporaire[i];
            }

            codeProgramme = true;
            demarrerClignotement(0, 255, 0, PROGRAMMATION_REUSSIE);
        }
        else
        {
            demarrerClignotement(255, 0, 0, RECOMMENCER_PROGRAMMATION);
        }

        return;
    }

    if (typeSaisie == OUVERTURE_COFFRE)
    {
        bool codeValide = true;

        for (int i = 0; i < 4; i++)
        {
            if (codeSaisi[i] != codeEnregistre[i])
            {
                codeValide = false;
                break;
            }
        }

        typeSaisie = AUCUNE_SAISIE;
        etatSysteme = ATTENTE;

        if (codeValide)
        {
            ouvrirCoffre();
        }
        else
        {
            demarrerClignotement(255, 0, 0, OUVERTURE_REFUSEE);
        }
    }
}

// Démarre un clignotement non bloquant avec millis().
void demarrerClignotement(uint8_t r, uint8_t g, uint8_t b, ActionClignotement action)
{
    couleurClignotementR = r;
    couleurClignotementG = g;
    couleurClignotementB = b;
    nombreTransitions = 0;
    dernierClignotement = millis();
    couleurAllumee = false;
    actionApresClignotement = action;
    etatSysteme = CLIGNOTEMENT;

    afficherCouleur(0, 0, 0);
}

// Gère le clignotement sans utiliser delay().
void gererClignotement()
{
    if (etatSysteme != CLIGNOTEMENT)
    {
        return;
    }

    if (millis() - dernierClignotement < DUREE_CLIGNOTEMENT)
    {
        return;
    }

    dernierClignotement = millis();
    couleurAllumee = !couleurAllumee;

    if (couleurAllumee)
    {
        afficherCouleur(couleurClignotementR, couleurClignotementG, couleurClignotementB);
    }
    else
    {
        afficherCouleur(0, 0, 0);
    }

    nombreTransitions++;
    if (nombreTransitions >= 6)
    {
        terminerClignotement();
    }
}

void terminerClignotement()
{

    // Ici selon l'état du clignotement on affiche une couleur 

    ActionClignotement action = actionApresClignotement;
    actionApresClignotement = AUCUNE_ACTION;
    etatSysteme = ATTENTE;

    if (action == PROGRAMMATION_REUSSIE)
    {
        afficherCouleur(0, 255, 0);
        coffreOuvert = true;
        journaliserEvenement("CHANGEMENT_CODE");
        Serial.println("PROGRAMMATION REUSSIE !");
    }
    else if (action == RECOMMENCER_PROGRAMMATION)
    {
        afficherCouleur(255, 80, 0);
        programmerNouveauCode();
    }
    else if (action == OUVERTURE_REFUSEE)
    {
        afficherCouleur(255, 0, 0);
        coffreOuvert = false;
        effacerAfficheur();
        Serial.println("Le coffre reste ferme.");
    }
}

// Déverrouille le coffre et journalise l'ouverture.
void ouvrirCoffre()
{
    if (coffreOuvert || !codeProgramme)
    {
        return;
    }

    servo.attach(SERVO_PIN, 500, 2400);
    servo.write(0);
    servoEnMouvement = true;
    debutMouvementServo = millis();

    afficherCouleur(0, 255, 0);
    effacerAfficheur();
    coffreOuvert = true;

    Serial.println("COFFRE OUVERT !");
    journaliserEvenement("OUVERTURE");
}

// Verrouille le coffre et journalise la fermeture.
void fermerCoffre()
{
    if (!codeProgramme || !coffreOuvert)
    {
        return;
    }

    servo.attach(SERVO_PIN, 500, 2400);
    servo.write(180);
    servoEnMouvement = true;
    debutMouvementServo = millis();

    afficherCouleur(255, 0, 0);
    effacerAfficheur();
    coffreOuvert = false;

    Serial.println("COFFRE FERME !");
    journaliserEvenement("FERMETURE");
}

// Arrête le servomoteur après la durée prévue.
void gererServo()
{
    if (servoEnMouvement && millis() - debutMouvementServo >= DUREE_SERVO)
    {
        servo.detach();
        servoEnMouvement = false;
        Serial.println("Mouvement du servo termine.");
    }
}

void programmerNouveauCode()
{
    if (!coffreOuvert)
    {
        Serial.println("Impossible de programmer : coffre ferme.");
        return;
    }

    for (int i = 0; i < 4; i++)
    {
        codeTemporaire[i] = 0;
        codeConfirmation[i] = 0;
    }

    afficherCouleur(255, 80, 0);
    demarrerSaisie(PROGRAMMATION_PREMIER_CODE);
}

void gererBoutonA()
{
    if (!appuiA)
    {
        return;
    }

    appuiA = false;

    if (etatSysteme == SAISIE_CODE)
    {
        incrementerChiffre();
        return;
    }

    if (etatSysteme == ATTENTE)
    {
        if (coffreOuvert && codeProgramme)
        {
            fermerCoffre();
        }
        else if (!coffreOuvert && codeProgramme)
        {
            demarrerSaisie(OUVERTURE_COFFRE);
        }
        else if (!codeProgramme)
        {
            Serial.println("Aucun code programme !");
        }
    }
}

void gererBoutonB()
{
    if (!appuiB)
    {
        return;
    }

    appuiB = false;

    if (etatSysteme == SAISIE_CODE)
    {
        confirmerChiffre();
    }
}

// Lit les boutons, applique l'anti-rebond et détecte l'appui long.
void lireBoutons()
{
    unsigned long maintenant = millis();

    bool nouvelleLectureA = digitalRead(BUTTON_A);
    if (nouvelleLectureA != lectureA)
    {
        lectureA = nouvelleLectureA;
        dernierChangementA = maintenant;
    }

    if (maintenant - dernierChangementA >= DEBOUNCE_DELAY)
    {
        if (lectureA != etatStableA)
        {
            etatStableA = lectureA;
            if (etatStableA == LOW)
            {
                appuiA = true;
            }
        }
    }

    bool nouvelleLectureB = digitalRead(BUTTON_B);
    if (nouvelleLectureB != lectureB)
    {
        lectureB = nouvelleLectureB;
        dernierChangementB = maintenant;
    }

    if (maintenant - dernierChangementB >= DEBOUNCE_DELAY)
    {
        if (lectureB != etatStableB)
        {
            etatStableB = lectureB;

            if (etatStableB == LOW)
            {
                tempsDebutAppuiB = maintenant;
                appuiLongDeclenche = false;
            }
            else
            {
                unsigned long dureeAppui = maintenant - tempsDebutAppuiB;
                if (!appuiLongDeclenche &&
                    dureeAppui < DUREE_APPUI_LONG &&
                    etatSysteme == SAISIE_CODE)
                {
                    appuiB = true;
                }
            }
        }
    }

    // L'appui long est détecté ici pour changer le code ou le programmer si aucun.
    if (etatStableB == LOW && !appuiLongDeclenche)
    {
        if (maintenant - tempsDebutAppuiB >= DUREE_APPUI_LONG)
        {
            appuiLongDeclenche = true;

            if (etatSysteme == ATTENTE && coffreOuvert)
            {
                programmerNouveauCode();
            }
            else if (!coffreOuvert)
            {
                Serial.println("Reprogrammation impossible : coffre ferme.");
            }
        }
    }
}

// ==========================================
// INITIALISATION
// ==========================================

void setup()
{
    Serial.begin(115200);

    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);

    matrix.begin();
    matrix.setBrightness(30);
    matrix.clear();
    matrix.show();

    afficheur.setBrightness(7, true);
    afficheur.clear();
    afficheur.showNumberDec(8888, true);

    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN, 500, 2400);
    servo.write(0);
    debutMouvementServo = millis();
    servoEnMouvement = true;

    codeProgramme = false;
    coffreOuvert = true;
    afficherCouleur(0, 255, 0);

    lectureA = digitalRead(BUTTON_A);
    etatStableA = lectureA;
    lectureB = digitalRead(BUTTON_B);
    etatStableB = lectureB;

    dernierChangementA = millis();
    dernierChangementB = millis();

    connecterWiFi();
    Serial.println("Initialisation terminee.");
}

// ==========================================
// BOUCLE PRINCIPALE
// ==========================================

void loop()
{
    lireBoutons();
    gererBoutonA();
    gererBoutonB();
    gererClignotement();
    gererServo();
    gererWiFiEtHeure();

}
