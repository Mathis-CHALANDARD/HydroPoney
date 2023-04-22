/*Appel des librairies*/
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include "secrets_arduino.h"
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

enum OperationState{
  pending,
  complete,
  interrupted
};


#pragma region LIB_INIT
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
#pragma endregion LIB_INIT

#pragma region MQTT_UTILS
int millisPrecedent = 0;
int millisActuel;
int intervalle = 600000;
#pragma endregion MQTT_UTILS

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

  while (WiFi.begin(WiFi_SSID, WiFi_PSWD) != WL_CONNECTED) {
    operation(pending);
  }
  operation(complete);

  if (!mqttClient.connect(MQTT_BRKR, MQTT_PORT)){
    operation(pending);
    while(1);
  }
  operation(complete);
  Serial.println("Connection mqtt réussie");
  mqttClient.onMessage(onMqttMessage);
  mqttClient.subscribe(MQTT_RECV);
  
  phPlus.attach(POMPEPHPLUS);
  phMoins.attach(POMPEPHMOINS);
}


void loop() {

  if (!mqttClient.connected()) {
    //No MQTT!
    while (WiFi.begin(WiFi_SSID, WiFi_PSWD) != WL_CONNECTED) {
      operation(pending);
    }
    Serial.println("Reconnexion wifi réussie");
    while (!mqttClient.connect(MQTT_BRKR, MQTT_PORT)){
      operation(pending);
    }
    mqttClient.onMessage(onMqttMessage);
    mqttClient.subscribe(MQTT_RECV);
    Serial.println("Reconnexion mqtt réussie");
  }

  mqttClient.poll();
  millisActuel = millis();
  /*Boucle qui s'active toute les 10 secondes. Envoie les données des capteurs à MQTT toutes les 10sec*/
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

    //Formatage des données
    String data = String(valLum0) + "/" + String(valLum1) + "/" + String(valLum2) + "/" + String(valLum3) + "/" + String(lumMoy) + "/" + 
                  String(tempGlobale) + "/" + String(hum) + "/" + String(pHVal) + "/" + String(ecVal) + '\n'; 

    //Envoyer les données a mqtt
    mqttClient.beginMessage(MQTT_SEND);
    mqttClient.print(data);
    mqttClient.endMessage();

    //Tester si le message à bien été envoyé
    millisPrecedent = millisActuel;
    Serial.println("message sent");
  }
  if(!pompeActive) return;

  if(pompePlus){
    phPlus.write(vitessePompe);
    delay(DELAIPOMPE);
    phPlus.write(0);
  }
  else {
    phMoins.write(vitessePompe);
    delay(DELAIPOMPE);
    phMoins.write(0);
  }

  pompeActive = false;
}

void operation(OperationState state){
  switch (state) {
    case pending:
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
      delay(1000);

    case complete:
      digitalWrite(LED_BUILTIN, HIGH);
      delay(3000);
      digitalWrite(LED_BUILTIN, LOW);
      delay(3000);

    case interrupted:
      for(int i = 0; i < 10; i++){
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
      }
  }
}
void onMqttMessage(int messageSize){
  // we received a message, print out the topic and contents
  Serial.println("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");
  char ledOrder[64];

  // use the Stream interface to print the contents
  int i = 0;
  while (mqttClient.available()) {
    ledOrder[i] = (char)mqttClient.read();
    i++;
  }
  
  if (ledOrder[0] == '1' || strcmp(ledOrder, "LED ON") == 0){
    Serial.println("LAMPE ALLUMEE");
  }
  else{
    Serial.println("LAMPE ETEINTE");
  }
}