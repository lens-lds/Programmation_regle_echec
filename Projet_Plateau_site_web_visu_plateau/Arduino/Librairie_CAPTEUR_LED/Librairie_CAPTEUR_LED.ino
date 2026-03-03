// Auteur Adrien et léni 
//programme banc de tes
//config : speed serial 115200 / PIN_LED : a modifer en fonction arduino uno et esp32

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "A31301.h"
#include "config.h"



//-----------variables globales------------//

#define LED_PIN     2   // Broche GPIO de l'ESP32 --> A4 et Arduino UNO --> 2
#define LED_COUNT    64   // Nombre de leds par module


Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
// Fonction pour remplir les pixels un par un
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}

void setuLED(uint8_t addr_led, uint32_t color) {
  strip.setPixelColor(tab_LED[addr_led]-1, color);
  strip.show();
  //delay(10);
}



void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial){
    Serial.println("pas de serial");
  }
  Serial.println("Setup");
  // I²C initialization
  Wire.begin();  

  strip.begin();           // Initialise la communication avec les LEDs
  strip.show();            // Éteint tout au démarrage
  strip.setBrightness(255); // Luminosité à environ 20% pour 50 (pour économiser le courant via USB)
}

void loop() {


  //-----------------------fonction test appelé par le prog python------------------------------------------//
if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "test_I2C") {
      // On envoie un header spécial pour le scan I2C
      Serial.write(0xCC); 
      Serial.write(0xDD);

      for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();

        if (error == 0) {
          Serial.write(address); // On envoie l'adresse trouvée
        }
      }
      Serial.write(0xFF); // Fin du scan
      delay(500);
      return;
    }

    if (command == "offset") {
      // On envoie un header spécial pour le scan I2C
      Serial.write(0xEE); 
      Serial.write(0xEE);

      for (uint8_t i = 0; i<64; i++) 
      {
        //-----moyen de la valeur a vide----//
        uint16_t moyenZ = 0;
        for (uint8_t m = 0; m<10; m++)
        {
          moyenZ += getZ(A31301_ADDR[i]);
        }
        moyenZ = moyenZ/10;
        A31301_ADDR[i]=moyenZ;
      }
      
      Serial.write(0xFF); // Fin du scan
      delay(500);
      return;
    }



  }

  
  //---------Gestion des cases du plateau-------- 
    //Serial.println("-----------------");
  Serial.write(0xAA); 
  Serial.write(0xBB);
  uint8_t checksum = 0;
  int16_t ValeurZ=0;
  //Etat = 0 (Noir), 1 (Blanc), 2 (Rien)
  uint8_t etat = 0;

    for(uint8_t k=0;k<8;k++){
      for(uint8_t j=0;j<8;j++){  
        if(presence_pion_blanc((j+(k*8)))){
          //Serial.print("| B ");
          setuLED((j+(k*8)),strip.Color(255, 255, 255));
          etat = 1;
        }
        else if(presence_pion_noir((j+(k*8)))){
          //Serial.print("| N ");
          setuLED((j+(k*8)),strip.Color(255, 255, 0));
          etat = 0;
        }
        else{
          //Serial.print("| - ");
          setuLED((j+(k*8)),strip.Color(0, 0, 0));
          etat = 2;
        }
        ValeurZ = getZ((j+(k*8)));
        uint8_t highZ = (ValeurZ >> 8) & 0xFF;
        uint8_t lowZ = ValeurZ & 0xFF;
        Serial.write((j+(k*8)));
        Serial.write(highZ);  // Valeur Z (partie haute)
        Serial.write(lowZ);   // Valeur Z (partie basse)
        Serial.write(etat);   // État du pion
        checksum += ((j+(k*8)) + highZ + lowZ + etat);
      }
      //Serial.print("|\n");
      //Serial.println("-----------------");  
    }
    Serial.write(checksum);

}