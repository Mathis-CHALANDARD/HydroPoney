/*Appel des librairies*/
#include <MKRWAN.h>/*Librairie pour se connecter au réseau LoRa*/
#include <DHT.h>/*Librairie pour récuperer les données du capteur de température*/
#include <DFRobot_EC_NO_EEPROM.h>/*Librairie pour récupérer les données du capteur d'électro-conductivité*/
#include <Servo.h>/*Libraire pour contrôler les pompes*/

#pragma region DEFINITION_CAPTEURS
/*Définition des pin de l'Arduino sur lesquelles lire les infos des capteurs*/
/* DFR0026 */       #define PIN_LUM0 A0
/* DFR0026 */       #define PIN_LUM1 A1
/* DFR0026 */       #define PIN_LUM2 A2
/* DFR0026 */       #define PIN_LUM3 A3
/* SEN0169 */       #define PIN_PH A4
/* SEN0137 */       #define PIN_TEMPHUM A6
/* DFR0300 */       #define PIN_ELEC A5
/*DHT 22 (AM2302)*/ #define DHTTYPE DHT22
/*PERISTALTIC PH+*/ #define POMPEPHPLUS 9
/*PERISTALTIC PH-*/ #define POMPEPHMOINS 10
/*DELAI*/           #define DELAIPOMPE 2000   
DHT dhtCaptor(PIN_TEMPHUM, DHT22); //Objet pour le capteur temperature + humidité
DFRobot_EC_NO_EEPROM elec; //Objet pour le capteur d'electro-conductivité
Servo phPlus;
Servo phMoins;
#pragma endregion DEFINITION_CAPTEURS


#pragma region LIB_INIT
/*Initailisation des variables pour les librairies*/
//DynamicJsonDocument jsonBuffer(1024);
LoRaModem modem;
#pragma endregion LIB_INIT

#pragma region TTN_UTILS
int millisPrecedent = 0;
int millisActuel;
int intervalle = 60000;
String appEui = "0000000000000063";
String appKey = "D1B8165EBC2E04DFAC5D023511CFB686";
#pragma endregion TTN_UTILS

#pragma region CAPTEURS_VAR
//Données relatives à la mesure de la température globale et de l'humidité
float tempGlobale = 0;
float hum = 0;

//Données relatives à la mesure de l'electroconductivité
float ecVoltage;
float ecVal;
float tempEau = 25; 

//Données relatives à la mesure de la luminosité globale
float lumMoy = 0;
float valLum0 = 0;
float valLum1 = 0;
float valLum2 = 0;
float valLum3 = 0;

//Données relatives au pH de l'eau
float pHVal;
float pHVoltage;

//Données liées à la pompe
int vitessePompe = 50;
unsigned long timer;
bool pompePlus = true;
bool pompeActive = true;
#pragma endregion CAPTEURS_VAR


void setup() {
  Serial.begin(9600);
  dhtCaptor.begin();
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);  
  /*Définition de la zone géographique*/
  if (!modem.begin(US915)) {
    Serial.println("Echec");
    while (1) {}
  };

  /*Connection à TheThingsNetwork*/
  int connected = modem.joinOTAA(appEui, appKey);

  if(connected == 0)
  {
    Serial.println("Connection effectuée !");
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  phPlus.attach(POMPEPHPLUS);
  phMoins.attach(POMPEPHMOINS);
}


void loop() {
  /*Boucle qui s'active toute les 10 secondes. Envoie les données des capteurs à TTN toutes les 10sec*/
  millisActuel = millis();
  if(millisActuel - millisPrecedent >= intervalle){    
    //Récupération des données de température globale et d'humidité
    tempGlobale = dhtCaptor.readTemperature(); 
    hum = dhtCaptor.readHumidity();

    //Récupération des données liées à l'éléctroconductivité
    ecVoltage = analogRead(PIN_ELEC)/1024.0*5000;//lecture du pHVoltage
    ecVal = elec.readEC(ecVoltage,tempEau);//Convertion du pHVoltage mesuré en éléctroconductivité avec une compensation en fonction de la température de l'eau
    elec.calibration(ecVoltage,tempEau);

    //Récupération des données de luminosité
    valLum0 = analogRead(PIN_LUM0);
    valLum1 = analogRead(PIN_LUM1);
    valLum2 = analogRead(PIN_LUM2);
    valLum3 = analogRead(PIN_LUM3);
    lumMoy = (valLum0 + valLum1 + valLum2 + valLum3)/4;

    //Récupération des données liées au pH 
    pHVoltage = analogRead(PIN_PH) * 5.0 / 1024;//Récupére le pHVoltage du capteur de pH
    pHVal = 3.5 * pHVoltage;

    if(pHVal < 5.8){
      //activer la pompe ph+ pendant 2 secondes
      pompeActive = true;
      pompePlus = true;
    }

    if(pHVal > 7.2){
      //activer la pompe ph- pendant 2 secondes
      pompeActive = true;
      pompePlus = false;
    }


    //Débuter la transmission
    modem.beginPacket();

    //Formatage des données
    String data = String(valLum0) + "/" + String(valLum1) + "/" + String(valLum2) + "/" + String(valLum3) + "/" + String(lumMoy) + "/" + 
                  String(tempGlobale) + "/" + String(hum) + "/" + String(pHVal) + "/" + String(ecVal) + '\n'; 

    //Envoie les données
    modem.print(data);

    //Tester si le message à bien été envoyé
    int erreur = modem.endPacket(true);
    if (erreur > 0)
    {      
      Serial.println("Message envoyé !");
      #pragma region Debogage
      for(int i = 0; i < 10; i++)
      {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
      }
      #pragma endregion
    } 
    else 
    {
      #pragma region Debogage
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
      #pragma endregion       
    }
    millisPrecedent = millisActuel;
  }
  if(!pompeActive) return;

  if(pompePlus){
    phPlus.write(vitesse);
    delay(DELAIPOMPE);
    phPlus.write(0);
  }
  else {
    phMoins.write(vitesse);
    delay(DELAIPOMPE);
    phMoins.write(0);
  }

  pompeActive = false;
}