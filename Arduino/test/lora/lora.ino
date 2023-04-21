#include <MKRWAN.h>/*Librairie pour se connecter au réseau LoRa*/
#include "secrets_arduino.h"
LoRaModem modem;
String appEui = APP_EUI;
String appKey = APP_KEY;


void setup() {
  Serial.begin(9600);
  Serial.println(appEui);
  Serial.println(appKey);

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
}

void loop() {
  // put your main code here, to run repeatedly:

}
