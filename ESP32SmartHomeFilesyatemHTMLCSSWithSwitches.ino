#include <Arduino.h>
#include <WiFi.h> //works for only esp32
#include <WiFiManager.h> //works for only esp32
#include <FirebaseESP8266.h> //works for both esp32 and esp8266
#include <ArduinoJson.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include "SPIFFS.h"
#include <FS.h>

#define DATA_PIN 12

#define S1_INPUT_PIN 14
#define S2_INPUT_PIN 27
#define S3_INPUT_PIN 26
#define S4_INPUT_PIN 25

#define BOOT_BUTTON_PIN 0

#define SWITCH1 16
#define SWITCH2 17
#define SWITCH3 18
#define SWITCH4 19

#define NUM_LEDS 1
#define CHIPSET WS2812
#define BRIGHTNESS 50
#define COLOR_ORDER GRB
#define STATUS_LED 0
CRGB leds[NUM_LEDS];

unsigned long previousMillis = 0;
const unsigned long interval = 50;

const int portalOpenTime = 300000; //server open for 5 mins
bool onDemand;

int prevStateofS1 = LOW;
int currentStateofS1;
int prevStateofS2 = LOW;
int currentStateofS2;
int prevStateofS3 = LOW;
int currentStateofS3;
int prevStateofS4 = LOW;
int currentStateofS4;

String firebaseStatus = "";

String roomId = ""; //change your room no according to mobile APP
unsigned int roomNo;


const char* switch1;
const char* switch2;
const char* switch3;
const char* switch4;

unsigned int s1;
unsigned int s2;
unsigned int s3;
unsigned int s4;

unsigned int prev_s1;
unsigned int prev_s2;
unsigned int prev_s3;
unsigned int prev_s4;

FirebaseData firebaseData;
AsyncWebServer server(80);
Preferences preferences;


TaskHandle_t Task1;

void setup() {
  Serial.begin(115200);


  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  pinMode(SWITCH1, OUTPUT);
  pinMode(SWITCH2, OUTPUT);
  pinMode(SWITCH3, OUTPUT);
  pinMode(SWITCH4, OUTPUT);

  pinMode(BOOT_BUTTON_PIN, INPUT);
  pinMode(S1_INPUT_PIN, INPUT);
  pinMode(S2_INPUT_PIN, INPUT);
  pinMode(S3_INPUT_PIN, INPUT);
  pinMode(S4_INPUT_PIN, INPUT);

  delay(100);

  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();


  connectWiFi();
  connectFirebase();
  storingFirebaseInitialValue();

  xTaskCreatePinnedToCore(
    loop1,
    "Task1",
    10000,
    NULL,
    1,
    &Task1,
    1);
  delay(500);
}


void connectFirebase() {
  preferences.begin("my-app", false);

  if (preferences.getString("firebaseUrl", "") != "" && preferences.getString("firebaseToken", "") != "") {
    Serial.println("Firebase settings already exist. Checking Firebase connection...");

    Firebase.begin(preferences.getString("firebaseUrl", ""), preferences.getString("firebaseToken", ""));
    roomId = preferences.getString("roomId", "");
    delay(100);
    Firebase.reconnectWiFi(true);
    delay(100);

    if (isFirebaseConnected() == true) {
      Serial.println("Connected to Firebase. Skipping server setup.");
      firebaseStatus = "ok";
    } else {
      Serial.println("Failed to connect to Firebase. Starting server setup.");
      setupServer();
    }
  } else {
    Serial.println("Firebase settings not found. Starting server setup.");
    setupServer();
  }

}
///////////////////////////////////////////////////////////////////////
void storingFirebaseInitialValue() {
  if (firebaseStatus == "ok") {
    Firebase.getString(firebaseData, "/ESPSmartHome/room" + roomId);
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, firebaseData.stringData());

    if (error) {
      //    Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    switch1 = doc["switch1"]; // "110"
    switch2 = doc["switch2"]; // "120"
    switch3 = doc["switch3"]; // "130"
    switch4 = doc["switch4"]; // "140"

    prev_s1 = atoi(switch1); // "110"
    prev_s2 = atoi(switch2); // "120"
    prev_s3 = atoi(switch3); // "130"
    prev_s4 = atoi(switch4); // "140"

  }
}
/////////////////////////////////////////////////////////////////////

void setupServer() {
  preferences.begin("my-app", false);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false);
  });

  server.on("/Submit", HTTP_POST, [](AsyncWebServerRequest * request) {

    String firebaseUrl = request->arg("url");
    String firebaseToken = request->arg("token");
    String roomId = request->arg("roomId");

    preferences.putString("firebaseUrl", firebaseUrl);
    preferences.putString("firebaseToken", firebaseToken);
    preferences.putString("roomId", roomId);

    Firebase.begin(firebaseUrl, firebaseToken);
    delay(100);
    Firebase.reconnectWiFi(true);
    delay(100);



    if (isFirebaseConnected() == true) {
      firebaseStatus = "ok";
      Serial.println("Firebase settings saved");
      delay(300);
      Serial.println("Success");
      delay(300);
      Serial.println("Restarting your device...");
      delay(500);
      ESP.restart();
    } else {
      firebaseStatus = "";
      Serial.println("Firebase settings saved");
      delay(300);
      Serial.println("Error! Check your credentials.");
      delay(300);
      Serial.println("Restarting your device...");
      delay(500);
      ESP.restart();
    }
  });

  server.serveStatic("/", SPIFFS, "/");
  server.begin();

  Serial.println("server begin");
  Serial.println(WiFi.localIP());

  showLedStatus(0, 0, 255);


  delay(portalOpenTime);
  Serial.println("Restarting your device...");
  delay(1000);
  ESP.restart();
}

void connectWiFi() {

  WiFiManager wm;
  WiFi.disconnect();
  delay(50);
  bool success = false;
  while (!success) {
    //    Serial.println("AP - espSmartHome  Setup IP - 192.168.4.1");
    wm.setConfigPortalTimeout(60);
    success = wm.autoConnect("espSmartHome");
    if (!success) {
      Serial.println("espSmartHome");
      Serial.println("Setup IP - 192.168.4.1");
      Serial.println("Conection Failed!");
    }
  }

  Serial.print("Connected SSID - ");
  Serial.println(WiFi.SSID());
  Serial.print("IP Address is : ");
  Serial.println(WiFi.localIP());
  delay(3000);
}

void onDemandFirebaseConfig() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    onDemand = true;
    firebaseStatus = "";
    setupServer();
  }
  delay(100);
}

void decodeData(String data) {

  //  Serial.println(data); //For Example=> {"value1":"\"on\"","value2":"\"on\"","value3":"\"off\"","value4":"\"off\""}

  /*
      goto website https://arduinojson.org/v6/assistant/#/step1
      select board
      choose input datatype
      and paste your JSON data
      it automatically generate your code
  */


  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    //    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  switch1 = doc["switch1"]; // "110"
  switch2 = doc["switch2"]; // "120"
  switch3 = doc["switch3"]; // "130"
  switch4 = doc["switch4"]; // "140"

  s1 = atoi(switch1); // "110"
  s2 = atoi(switch2); // "120"
  s3 = atoi(switch3); // "130"
  s4 = atoi(switch4); // "140"



  //  Serial.print("Room No:" + roomId + "  ");
  //  Serial.print(switch1);
  //  Serial.print("  ");
  //  Serial.print(switch2);
  //  Serial.print("  ");
  //  Serial.print(switch3);
  //  Serial.print("  ");
  //  Serial.println(switch4);

  Serial.print("Room No:" + roomId + "  ");
  Serial.print(s1);
  Serial.print("  ");
  Serial.print(s2);
  Serial.print("  ");
  Serial.print(s3);
  Serial.print("  ");
  Serial.println(s4);


}

void controlSwitch1(unsigned int inValue) {

  if (inValue == (roomNo * 100 + 1 * 10 + 1)) {
    digitalWrite(SWITCH1, HIGH);
  }
  else if (inValue == (roomNo * 100 + 1 * 10 + 0)) {
    digitalWrite(SWITCH1, LOW);
  }
}
void controlSwitch2(unsigned int inValue) {
  if (inValue == (roomNo * 100 + 2 * 10 + 1)) {
    digitalWrite(SWITCH2, HIGH);
  }
  else if (inValue == (roomNo * 100 + 2 * 10 + 0)) {
    digitalWrite(SWITCH2, LOW);
  }
}
void controlSwitch3(unsigned int inValue) {
  if (inValue == (roomNo * 100 + 3 * 10 + 1)) {
    digitalWrite(SWITCH3, HIGH);
  }
  else if (inValue == (roomNo * 100 + 3 * 10 + 0)) {
    digitalWrite(SWITCH3, LOW);
  }
}

void controlSwitch4(unsigned int inValue) {
  if (inValue == (roomNo * 100 + 4 * 10 + 1)) {
    digitalWrite(SWITCH4, HIGH);
  }
  else if (inValue == (roomNo * 100 + 4 * 10 + 0)) {
    digitalWrite(SWITCH4, LOW);
  }
}

boolean isFirebaseConnected() {
  Firebase.getString(firebaseData, "/ESPSmartHome");
  if (firebaseData.stringData() != "") {
    return true;
  }
  else {
    return false;
  }
}



void showLedStatus(uint8_t r, uint8_t g, uint8_t b ) {
  leds[STATUS_LED] = CRGB(r, g, b);;
  FastLED.show();
}

///////////////////////////////////////////////////////////////

void loading()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
  }
}



///////////////////////////////////////////////////////////////
void loop1(void * parameter) {

  while (1) {

    if (WiFi.status() == WL_CONNECTED && firebaseStatus == "ok") {
      showLedStatus(0, 255, 0);
    }

    if (onDemand == true) {
      //      showLedStatus(0, 0, 255);
      loading();
      FastLED.show();
    }
    if (WiFi.status() != WL_CONNECTED) {
      showLedStatus(255, 0, 0);
      connectWiFi();
    }
  }
}

//////////////////////////////////////////////////////////////

int whichOneisChanged() {
  currentStateofS1 = digitalRead(S1_INPUT_PIN);
  currentStateofS2 = digitalRead(S2_INPUT_PIN);
  currentStateofS3 = digitalRead(S3_INPUT_PIN);
  currentStateofS4 = digitalRead(S4_INPUT_PIN);
  if (firebaseStatus == "ok") {
    Firebase.getString(firebaseData, "/ESPSmartHome/room" + roomId);
    decodeData(firebaseData.stringData());

    if (s1 != prev_s1 || s2 != prev_s2 || s3 != prev_s3 || s4 != prev_s4) {
      prev_s1 = s1;
      prev_s2 = s2;
      prev_s3 = s3;
      prev_s4 = s4;
      return 2;
    }

  }
  else {
    Serial.println("firebase failed");
    return 1;
  }

  if (currentStateofS1 != prevStateofS1 || currentStateofS2 != prevStateofS2 || currentStateofS3 != prevStateofS3 || currentStateofS4 != prevStateofS4) {
    prevStateofS1 = currentStateofS1;
    prevStateofS2 = currentStateofS2;
    prevStateofS3 = currentStateofS3;
    prevStateofS4 = currentStateofS4;
    return 1; //return 1 for control by switches
  }
}

//////////////////////////////////////////////////////////////////
void controlSwitches(int caseValue) {
  if (caseValue == 1) {
    controlBySwitches();
  }
  if (caseValue == 2) {
    controlSwitch1(s1);
    controlSwitch2(s2);
    controlSwitch3(s3);
    controlSwitch4(s4);
  }


}
/////////////////////////////////////////////////////////////
void controlBySwitches() {
  digitalWrite(SWITCH1, currentStateofS1);
  digitalWrite(SWITCH2, currentStateofS2);
  digitalWrite(SWITCH3, currentStateofS3);
  digitalWrite(SWITCH4, currentStateofS4);
}

//////////////////////////////////////////////////////////////

void loop() {

  onDemand = false;
  onDemandFirebaseConfig();
  roomNo = String(roomId).toInt();


  controlSwitches(whichOneisChanged());



  //  if (firebaseStatus == "ok") {
  //    Firebase.getString(firebaseData, "/ESPSmartHome/room" + roomId);
  //    decodeData(firebaseData.stringData());
  //
  //    controlSwitch1(s1);
  //    controlSwitch2(s2);
  //    controlSwitch3(s3);
  //    controlSwitch4(s4);
  //  }
  //  else {
  //    Serial.println("firebase failed");
  //  }
  //

  if (firebaseStatus != "ok") {
    if (WiFi.status() == WL_CONNECTED) {
      Firebase.getString(firebaseData, "/ESPSmartHome/room" + roomId);
      decodeData(firebaseData.stringData());
    }
  }

}
