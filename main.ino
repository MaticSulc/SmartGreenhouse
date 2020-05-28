#include "WiFi.h"   //WiFi povezava
#include "ESPAsyncWebServer.h"  //Streznik za spletno stran
#include "DHT.h"    //Senzor za temperaturo in vlago
#include "time.h"   //Beleženje časa
#include "AsyncJson.h"  //pretvorba v JSON obliko
#include "ArduinoJson.h"  //pretvorba v JSON obliko
#include "FS.h"           //uporaba SPIFFS
#include "SPIFFS.h"       //uporaba SPIFFS
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
/*------------------------------------------------------------------------------*/  
#define lightsPin 23                                          //luči
#define pumpPin 32                                            //črpalka
#define fanPin 33                                             //ventilator
#define soilSensorPin 34                                      //senzor za vlažnost
#define soilSensorPinGnd 4                                    //ground senzorja za vlažnost
#define dhtType DHT22                                         //tip senzorja
#define dhtPin 27                                             //senzor za temperaturo
#define RELAY_NUM 3                                           //število relejev za gumbe

const char * ntpServer = "de.pool.ntp.org"; //nemški NTP strežnik
const long gmtCona = 3600;            //3600s = 1h, Slovenija ima GMT+1
const int premikUre = 3600;           //poletni čas (trenutno) je 1h(3600) naprej, zimski je 0
unsigned long toggleTime;                                     //čas od zagona programa
const long intervalSenzor = 5000;                             //interval branja senzorjev
const long intervalLuc = 43200000;                            //kako dolgo je vklopljena luč, privzeto je 12h oz. 43200000s
float soilMoisture = 0;                                       //vlažnost zemlje za odpravo popačenja
String lastTemperature;                                       //temperatura
String lastHumidity;                                          //vlažnost
String lastSoilMoisture;                                      //vlažnost zemlje
String lastWaterTime = "Še ni bilo zalito.";                  //zadnji čas zalivanja (ob zagonu izpiše, da še ni bilo zalito)
String lastTime;                                              //trenutni čas
bool manualOverride = false;                                  //ročna kontrola
unsigned long previousMillis = 0;                             //spremenljivka za beleženje časa
static unsigned char pumpState = LOW;                         //status črpalke
static unsigned long pumpCameOn = 0;                          //čas zadnjega vklopa črpalke
const char * ssid = "";                                   //ime wifi omrežja
const char * password = "";                         //geslo wifi omrežja
unsigned long check_wifi = 30000;
int relayGPIOs[RELAY_NUM] = {fanPin, pumpPin, lightsPin};     //priključki vseh naprav

//prototipi funkcij
void trenutenCas();
void procesirajPodatke(String msg1, String msg2);
void automate();

//objekti in strukture
struct tm timeinfo;                                           //struktura za beleženje časa
DHT dht(dhtPin, dhtType);                                    //object za DHT senzor
AsyncWebServer server(80);                                   //objekt za spletni strežnik

/*------------------------------------------------------------------------------*/
void setup() {
WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
   Serial.begin(115200);
   toggleTime = millis(); //lights
   SPIFFS.begin();
   dht.begin();

   pinMode(soilSensorPin, INPUT);

   for (int i = 1; i <= RELAY_NUM; i++) {
      pinMode(relayGPIOs[i - 1], OUTPUT);
      digitalWrite(relayGPIOs[i - 1], HIGH);
   }
   WiFi.setSleep(false);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi..");
   }

   server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {           //ko uporabnik pride na spletno stran 
      request -> send(SPIFFS, "/index.html", "text/html");
   });
   server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request) {  //ko uporabnik zahteva script.js
      request -> send(SPIFFS, "/script.js", "text/javascript");
   });

   server.on("/update", HTTP_GET, [](AsyncWebServerRequest * request) {
      String inputMessage1;
      String inputMessage2;
      if (request -> hasParam("relay") & request -> hasParam("state")) {
         inputMessage1 = request -> getParam("relay") -> value();
         inputMessage2 = request -> getParam("state") -> value();
         procesirajPodatke(inputMessage1, inputMessage2);
      } else {
         inputMessage1 = "Error";
         inputMessage2 = "Error";
      }
      request -> send(200, "text/plain", "OK");
   });

   server.on("/api/greenhouse", HTTP_GET, [](AsyncWebServerRequest * request) {
      AsyncResponseStream *response = request -> beginResponseStream("application/json");
      DynamicJsonBuffer jsonBuffer;
      JsonObject & root = jsonBuffer.createObject();
      root["temperature"] = lastTemperature;
      root["wateringtime"] = lastWaterTime;
      root["humidity"] = lastHumidity;
      root["time"] = lastTime;
      root["moisture"] = lastSoilMoisture;
      root["pumpState"] = !digitalRead(pumpPin);
      root["lightState"] = !digitalRead(lightsPin);
      root["fanState"] = !digitalRead(fanPin);
      root["manualoverride"] = manualOverride;
      root.printTo(*response);
      response -> addHeader("Access-Control-Allow-Origin", "*");
      request -> send(response);
   });

   server.begin();
   configTime(gmtCona, premikUre, ntpServer);
   trenutenCas();

}
void loop() {

  if ((WiFi.status() != WL_CONNECTED) && (millis() > check_wifi)) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    check_wifi = millis() + 30000;
  }
  
   unsigned long currentMillis = millis();                  //zabelezimo trenutni cas
   if (currentMillis - previousMillis >= intervalSenzor) {  //testiramo, če je minilo 5 sekund
      pinMode(soilSensorPinGnd,OUTPUT);
      digitalWrite(soilSensorPinGnd, LOW);                  //vklop senzorja vlage v zemlji
      previousMillis = currentMillis;                       //nastavimo prejšnji čas na trenutnega
      float humidity = dht.readHumidity();                  //branje vlažnosti s knjižnjico DHT
      float temperature = dht.readTemperature();            //branje temperature s knjižnjico DHT
      lastTemperature = String(temperature);                //pretvorba v string za JSON
      lastHumidity = String(humidity);                      //pretvorba v string za JSON
      for (int i = 0; i <= 100; i++) {                      //odprava napak pri branju
         soilMoisture += analogRead(soilSensorPin);         //da odpravimo popačeno vrednost vzamemo povprečje stotih vrednosti
         delay(1);                                          //1ms zamika potrebuje senzor za pravilno delovanje
      }
      soilMoisture /= 100;
      lastSoilMoisture = soilMoisture;                      //ne pretvarjamo v string, saj bomo podatek še obdelali
      soilMoisture = 0;
      pinMode(soilSensorPinGnd, INPUT);                 //izklop senzorja
      trenutenCas();                                        
   }
   automate();
}

void trenutenCas() {
   if (!getLocalTime( & timeinfo)) {          
      Serial.println("Failed to obtain time");                                             //napaka
      return;
   }
   char timeStringBuff[50];                                                                //50 znakov za buffer
   strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", & timeinfo);  //zapis časovne značke
   String asString(timeStringBuff);                                                        //pretvorba v String
   lastTime = timeStringBuff;                                                              
}

void procesirajPodatke(String msg1, String msg2) {
   if (msg1.toInt() == 4) {
      manualOverride = msg2.toInt();
   } else {
      digitalWrite(relayGPIOs[msg1.toInt() - 1], !msg2.toInt());
      if (relayGPIOs[msg1.toInt() - 1] == pumpPin && msg2.toInt() == 1) {
         pumpState = HIGH;
         pumpCameOn = millis();
      }
   }
}

void automate() {
   if (digitalRead(pumpPin) == LOW) {                                         //če je črpalka vključena, zabeležimo čas
      lastWaterTime = "Zadnje zalivanje: " + lastTime;
   }
   if (!manualOverride) {
      if (pumpState == HIGH) {                                                //avtomatski izklop črpalke po 7 sekundah in čakanje 5 sekund
         if (millis() - pumpCameOn > 7000) {
            digitalWrite(pumpPin, HIGH);
            pumpState = LOW;
         }
      }
      if (lastSoilMoisture.toInt() > 2500) {                                  //vklop črpalke, če vlažnost pade pod želeno se vklopi črpalka
         pumpState = HIGH;
         pumpCameOn = millis();                                               //beležimo vklop črpalke
         digitalWrite(pumpPin, LOW);
      }

      if ((lastHumidity.toInt() > 60 || lastTemperature.toInt() > 30)) {     //vklop ventilatorja, če je vlažnost nad 60% oz. temperatura nad 30 stopinj
         digitalWrite(fanPin, LOW);
      } else {
         digitalWrite(fanPin, HIGH);
      }

      if (millis() - toggleTime > intervalLuc) {                             //luči morajo biti vklopljene natanko 12 ur
         digitalWrite(lightsPin, !digitalRead(lightsPin)); //toggle
         toggleTime = millis();
      }

   }

}
