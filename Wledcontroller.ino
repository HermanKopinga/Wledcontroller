/*
 * Wled controller in hardware, by herman@kopinga.nl
 * 
 * Runs (with some curious warnings and something akin to a power problem) on ESP v 2.0.8 and TFT_eSPI 2.5.0 with Adafruit Neopixel as pixel driver.
 * Potential problem: pin is not reliably switching for some ESP32 reason.
 */

#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>  // For the three local LEDS to show selected color.
#include <TFT_eSPI.h>           // Hardware-specific library
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>            // Include the I2C library (required)
#include <SparkFunSX1509.h>  // Click here for the library: http://librarymanager/All#SparkFun_SX1509
#include <ArduinoJson.h>

#include "config.h" // to keep wifi passwords out of github
#include "heart.h"
#include "smiley.h"
#include "gear.h"
#include "pallete.h"

const byte SX1509_ADDRESS = 0x3E;  // SX1509 I2C address
#define sdaPin 26
#define sclPin 22
const byte SX1509_WHITELED1 = 4;  // LED connected to 4 (draining current)
const byte SX1509_WHITELED2 = 5;  // LED connected to 5 (draining current)
const byte SX1509_JUP = 15;       // Directional pad
const byte SX1509_JDOWN = 14;     // Directional pad
const byte SX1509_JLEFT = 13;     // Directional pad
const byte SX1509_JRIGHT = 12;    // Directional pad
const byte SX1509_JMID = 11;      // Directional pad
const byte SX1509_GA = 6;         // Big blue button
const byte SX1509_WTOP = 8;       // Top white button
const byte SX1509_WBOTTOM = 9;    // Bottom white button
const byte SX1509_ENCODER = 10;   // Encoder button
const byte SX1509_4067_S0 = 0;    // 4067 analog multiplexer
const byte SX1509_4067_S1 = 1;    // 4067 analog multiplexer
const byte SX1509_4067_S2 = 2;    // 4067 analog multiplexer
const byte SX1509_4067_S3 = 3;    // 4067 analog multiplexer

const byte SX1509_INTERRUPT_PIN = 27;

SX1509 io;  // Create an SX1509 object to be used throughout

#define backlightpin 4  // LCD backlight

TFT_eSPI tft = TFT_eSPI();  // Display driver

// Color is RGB565, 16-bit, that includes Red, Green and Blue in a 16-bit variable.
// The way the color is packed in is the top 5 bits are red, the middle 6 bits are green and the bottom 5 bits are blue.
// Comptabile colorpicker: https://rgbcolorpicker.com/565
#define TFT_MYGREY 0x5AEB
#define TFT_MYBACK 0x1042
#define TFT_MYACCENT 0x30C6
#define TFT_MYBORDER 0x514A
#define TFT_MYORANGE 0xFC80

#define ACCENTED_COLORS 1
#define ACCENTED_EFFECTS 2
#define ACCENTED_FAVORITES 3
#define ACCENTED_SETTINGS 4


// Timing stuff
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long millisBreak = 100;
unsigned long previousHeartbeat = 0;
unsigned long heartbeatBreak = 1000;

unsigned long sentMillis = 0;
unsigned long receivedMillis = 0;

byte buttonChanges = 1;
byte encoderChanges = 1;
byte faderChanges = 1;
byte status = 0;

// Preview led stuff:
// In a more ambitious moment this would have been a 2 dimensional array:
int r1 = 0;
int g1 = 0;
int b1 = 0;
int r2 = 0;
int g2 = 0;
int b2 = 0;
int r3 = 0;
int g3 = 0;
int b3 = 0;

// From WLED source at https://github.com/Aircoookie/WLED/blob/332be7edd69853f70aebf92b83a4f0f275bcfa96/wled00/colors.cpp#L305
// gamma 2.8 lookup table used for color correction
uint8_t static WLEDgamma[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

#define NUMPIXELS 10 // There are only 3 visible but because of sourcing thingies I've mounted 10.
#define LEDS_PIN 33
Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);


// Analog 4067 stuff
byte b;
int analog_state;
int analog_stored_state[15];
int analog_threshold = 15;
int scaled_value;
int analog_scale = 41;
const byte analog4067pin = 32;
const byte analogConnected = 15;

// interrupt service routine, needs IRAM_ATTR on esp32.
void IRAM_ATTR processButtons() {
  buttonChanges = 1;
}

// JSON smutz
String serverName1 = "http://192.168.2.";
String serverName2 = ":80/json/state";
// Voor Maakplek String serverName1 = "http://10.11.11.";
char serializedJson[255];  // String to put the result in before transmitting.

char const *deviceNames[] = { "Tester",
                              "Stok",
                              "Bank",
                              "Jongens",
                              "Decoratie",
                              "Kerstboom" };

char const *deviceIPs[] = { "91",
                            "43",
                            "39",
                            "137",
                            "47",
                            "89" };
byte currentDevice = 0;

bool wifi = 0;

// Local values to be sent remote
byte brightness = 0;
bool power = 1;

// User interface stuff
#define INTERFACEEFFECTS  0
#define INTERFACESETTINGS 1
#define INTERFACEFAVOURITES 2 // not used yet
int interfaceMode = INTERFACEEFFECTS;

// For the heartbeat led
bool heartBeatStatus = 1;

// Effect stuff
int currentEffect = 0;
char const *effectList[] = { "Solid",
                             "Blink",
                             "Breathe",
                             "Wipe",
                             "Wipe Random",
                             "Random Colors",
                             "Sweep",
                             "Dynamic",
                             "Colorloop",
                             "Rainbow",
                             "Scan",
                             "Scan Dual",
                             "Theater",
                             "Fade",
                             "Theater Rainbow",
                             "Running" };


void displayDraw() {
  tft.fillScreen(TFT_MYBACK);
  // Header
  tft.fillRect(0, 0, 239, 36, TFT_MYBORDER);
  // Side bar (excluding top part)
  tft.fillRect(0, 36, 47, 319, TFT_MYBORDER);
}

void ledSwoop() {
  for (int i = 0; i < NUMPIXELS; i++) {  // For each pixel...

    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    // Here we're using a moderately bright green color:
    pixels.setPixelColor(i, pixels.Color(0, 150, 0));

    pixels.show();  // Send the updated pixel colors to the hardware.

    delay(500);  // Pause before next pass through loop
  }
}

inline void displayCommState(char const *message, int clear = 1) {
  if (clear == 2) {
    tft.fillRect(47, 304, 192, 15, TFT_MYORANGE);
    tft.setTextColor(TFT_MYBACK, TFT_MYORANGE);
  } else {
    tft.fillRect(47, 304, 192, 15, TFT_MYBACK);
    tft.setTextColor(TFT_MYORANGE, TFT_MYBACK);    
  }
  tft.setTextSize(1);
  if (clear) {
    tft.setCursor(200, 307);
  }
  tft.print(message);
  tft.setTextSize(2);
}

void displayDeviceState() {
  // Redraw state of device, on or off.
  /*tft.setCursor(5,55);
  tft.setTextSize(2);
  tft.setTextColor(TFT_MYORANGE, TFT_MYBORDER);
  if (power) {
    tft.print("aan");
  } else {
    tft.print("uit");
  }*/
  io.digitalWrite(SX1509_WHITELED1, !power);
}

void displayIcons(int accented) {
  // Redraw side bar (excluding top part)
  tft.fillRect(0, 100, 47, 319, TFT_MYBORDER);

  // Now draw icons, accented is the selected 'page'.
  if (accented == 1) {
    tft.fillRect(5, 179 + ((accented - 1) * 31), 37, 33, TFT_MYORANGE);
  }
  if (accented == 2) {
    tft.fillRect(5, 214, 37, 33, TFT_MYORANGE);
  }
  if (accented == 3) {
    tft.fillRect(5, 249, 37, 33, TFT_MYORANGE);
  }
  if (accented == 4) {
    tft.fillRect(5, 281, 37, 31, TFT_MYORANGE);
  }
  tft.pushImage(8, 180, 31, 31, palleteimg, 0x00);  // colors
  tft.pushImage(8, 215, 30, 31, smiley, 0x00);      // effects
  tft.pushImage(8, 250, 31, 27, heart, 0x00);       // favorites
  tft.pushImage(9, 282, 29, 29, gear, 0x00);        // settings / debug
}

void displayEffects() {
  displayIcons(ACCENTED_EFFECTS);
  tft.setViewport(55, 43, 184, 261);
  tft.setTextColor(TFT_MYORANGE, TFT_MYBACK);
  tft.fillScreen(TFT_MYBACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);

  for (int i=0; i < sizeof(effectList)/4;i++){
    if (i == currentEffect) {
      tft.setTextColor(TFT_MYBACK, TFT_MYORANGE);
    }
    tft.println(effectList[i]);
    if (i == currentEffect) {
      tft.setTextColor(TFT_MYORANGE, TFT_MYBACK);
    }
  }
  tft.resetViewport();
}

void displayHeader() {
  tft.fillRect(0, 0, 239, 36, TFT_MYBORDER);
  tft.setCursor(5, 5);
  tft.setTextSize(4);
  tft.setTextColor(TFT_MYORANGE, TFT_MYBORDER);
  tft.print(deviceNames[currentDevice]);
  tft.setTextSize(2);
}

void displaySettings(bool clearBack) {
  // Only needed on first run of this.
  displayIcons(ACCENTED_SETTINGS);
  tft.setViewport(55, 43, 184, 261);
  if (clearBack) {
    tft.fillScreen(TFT_MYBACK);
  }
  tft.setCursor(10, 10);
  tft.printf("0 %03d", analog_stored_state[0] / 16);
  tft.setCursor(10, 30);
  tft.printf("1 %03d", analog_stored_state[1] / 16);
  tft.setCursor(10, 50);
  tft.printf("2 %03d", analog_stored_state[2] / 16);
  tft.setCursor(10, 70);
  tft.printf("3 %03d", analog_stored_state[3] / 16);
  tft.setCursor(10, 90);
  tft.printf("4 %03d", analog_stored_state[4] / 16);
  tft.setCursor(10, 110);
  tft.printf("5 %03d", analog_stored_state[5] / 16);
  tft.setCursor(10, 130);
  tft.printf("6 %03d", analog_stored_state[6] / 16);
  tft.setCursor(80, 10);
  tft.printf("7 %03d", analog_stored_state[7] / 16);
  tft.setCursor(80, 30);
  tft.printf("8 %03d", analog_stored_state[8] / 16);
  tft.setCursor(80, 50);
  tft.printf(" 9 %03d", analog_stored_state[9] / 16);
  tft.setCursor(80, 70);
  tft.printf("10 %03d", analog_stored_state[10] / 16);
  tft.setCursor(80, 90);
  tft.printf("11 %03d", analog_stored_state[11] / 16);
  tft.setCursor(80, 110);
  tft.printf("12 %03d", analog_stored_state[12] / 16);
  tft.setCursor(80, 130);
  tft.printf("R1 %03d", r1);

  /*tft.setCursor(10, 220);
  tft.print(io.digitalRead(SX1509_JUP));
  tft.print(io.digitalRead(SX1509_JDOWN));
  tft.print(io.digitalRead(SX1509_JLEFT));
  tft.print(io.digitalRead(SX1509_JRIGHT));
  tft.print(io.digitalRead(SX1509_JMID));*/
  tft.resetViewport();
}

void multiplexerSetup() {
  // The SX1509 interrupt is active-low.
  pinMode(SX1509_INTERRUPT_PIN, INPUT_PULLUP);

  Wire.begin(sdaPin, sclPin);

  // Call io.begin(<address>) to initialize the SX1509. If it
  // successfully communicates, it'll return 1.
  if (io.begin(SX1509_ADDRESS) == false) {
    Serial.println("Failed to communicate. Check wiring and address of SX1509.");
    digitalWrite(22, HIGH);  // If we failed to communicate, turn the pin 13 LED on
    while (1)
      ;  // If we fail to communicate, loop forever.
  }

  io.debounceTime(20);  // Set debounce time to 32 ms.

  io.pinMode(SX1509_WHITELED1, OUTPUT);
  io.pinMode(SX1509_WHITELED2, OUTPUT);
  io.pinMode(SX1509_JUP, INPUT_PULLUP);
  io.pinMode(SX1509_JDOWN, INPUT_PULLUP);
  io.pinMode(SX1509_JLEFT, INPUT_PULLUP);
  io.pinMode(SX1509_JRIGHT, INPUT_PULLUP);
  io.pinMode(SX1509_JMID, INPUT_PULLUP);
  io.pinMode(SX1509_GA, INPUT_PULLUP);
  io.pinMode(SX1509_WTOP, INPUT_PULLUP);
  io.pinMode(SX1509_WBOTTOM, INPUT_PULLUP);
  io.pinMode(SX1509_ENCODER, INPUT_PULLUP);
  io.pinMode(SX1509_4067_S0, OUTPUT);
  io.pinMode(SX1509_4067_S1, OUTPUT);
  io.pinMode(SX1509_4067_S2, OUTPUT);
  io.pinMode(SX1509_4067_S3, OUTPUT);

  io.enableInterrupt(SX1509_JUP, CHANGE);
  io.enableInterrupt(SX1509_JDOWN, CHANGE);
  io.enableInterrupt(SX1509_JLEFT, CHANGE);
  io.enableInterrupt(SX1509_JRIGHT, CHANGE);
  io.enableInterrupt(SX1509_JMID, CHANGE);
  io.enableInterrupt(SX1509_GA, CHANGE);
  io.enableInterrupt(SX1509_WTOP, CHANGE);
  io.enableInterrupt(SX1509_WBOTTOM, CHANGE);
  io.enableInterrupt(SX1509_ENCODER, CHANGE);

  io.debouncePin(SX1509_JUP);      // Enable debounce
  io.debouncePin(SX1509_JDOWN);    // Enable debounce
  io.debouncePin(SX1509_JLEFT);    // Enable debounce
  io.debouncePin(SX1509_JRIGHT);   // Enable debounce
  io.debouncePin(SX1509_JMID);     // Enable debounce
  io.debouncePin(SX1509_GA);       // Enable debounce
  io.debouncePin(SX1509_WTOP);     // Enable debounce
  io.debouncePin(SX1509_WBOTTOM);  // Enable debounce
  io.debouncePin(SX1509_ENCODER);  // Enable debounce

  attachInterrupt(digitalPinToInterrupt(SX1509_INTERRUPT_PIN), processButtons, FALLING);

  // Blink the LED a few times before we start:
  for (int i = 0; i < 2; i++) {
    // Use io.digitalWrite(<pin>, <LOW | HIGH>) to set an
    // SX1509 pin either HIGH or LOW:
    io.digitalWrite(SX1509_WHITELED1, HIGH);
    io.digitalWrite(SX1509_WHITELED2, HIGH);
    delay(100);
    io.digitalWrite(SX1509_WHITELED1, LOW);
    io.digitalWrite(SX1509_WHITELED2, LOW);
    delay(100);
    io.digitalWrite(SX1509_WHITELED1, HIGH);
    io.digitalWrite(SX1509_WHITELED2, HIGH);
  }
  io.pinMode(SX1509_WHITELED1, ANALOG_OUTPUT);
}

void setup() {
  pinMode(backlightpin, OUTPUT);
  pinMode(sdaPin, OUTPUT);
  pinMode(sclPin, OUTPUT);
  pinMode(analog4067pin, INPUT);
  digitalWrite(backlightpin, 1);
  delay(400);
  digitalWrite(backlightpin, 0);
  delay(400);
  digitalWrite(backlightpin, 1);

  Serial.begin(115200);
  Serial.println("Herman was here... ");
  Serial.println(millis());

  pixels.begin();
  pixels.show();  // Initialize all leds 'off'.
  //ledSwoop();

  Serial.println("led done");

  // SX1509 gets its own setup function.
  multiplexerSetup();

  // Display stuff
  tft.begin();
  tft.setRotation(2);

  displayDraw();
  displayCommState("on");
  Serial.println(millis());

  // Turn on Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin("Lief delen, niet stelen.", WIFIPASSWORD);
  //WiFi.begin("Maakplek", WIFIPASSWORD);
  Serial.print("Connecting to WiFi ..");
  for (int i = 0; i < 5; i++) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      displayCommState(".", 0);
      delay(1000);
      wifi = 0;
    } else {
      wifi = 1;
      Serial.println(WiFi.localIP());
      displayCommState("wifi");
      break;
    }
  }

  Serial.println(millis());
  displayEffects();
  displayHeader();
  displayDeviceState();
  Serial.println(deviceNames[currentDevice]);
  Serial.println(millis());
}

void serializeInput () {
  // Allocate the JSON document
  //
  // Inside the brackets, 1000 is the RAM allocated to this document.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  // Capacity required was initially 384, so 1000 should do nicely for a while :)

  // Documentation for WLED JSON api at:  https://kno.wled.ge/interfaces/json-api/
  StaticJsonDocument<1000> commandJson;
  JsonObject seg_0 = commandJson["seg"].createNestedObject();
  JsonArray seg_0_col = seg_0.createNestedArray("col");
  if (power) { 
    commandJson["on"] = true;
  } else { 
    commandJson["on"] = false;
  }
  commandJson["bri"] = 255;
  seg_0["fx"] = currentEffect;
  seg_0["sx"] = analog_stored_state[4]/16;
  
  JsonArray seg_0_col_0 = seg_0_col.createNestedArray();
  seg_0_col_0.add(r1);
  seg_0_col_0.add(g1);
  seg_0_col_0.add(b1);
  JsonArray seg_0_col_1 = seg_0_col.createNestedArray();
  seg_0_col_1.add(r2);
  seg_0_col_1.add(g2);
  seg_0_col_1.add(b2);
  JsonArray seg_0_col_2 = seg_0_col.createNestedArray();
  seg_0_col_2.add(r3);
  seg_0_col_2.add(g3);
  seg_0_col_2.add(b3);
  serializeJson(commandJson, serializedJson);
}

void readFaders() {
  // Borrowed from earlier work on musicmonster
  // maybe add some running averaging? If fast enough
  for (b = 0; b < analogConnected; b++) {
    // select the bit
    io.digitalWrite(SX1509_4067_S0, bitRead(b, 0));
    io.digitalWrite(SX1509_4067_S1, bitRead(b, 1));
    io.digitalWrite(SX1509_4067_S2, bitRead(b, 2));
    io.digitalWrite(SX1509_4067_S3, bitRead(b, 3));

    analog_state = analogRead(analog4067pin);

    // This voodoo malarky needs some documentation.
    if (analog_state - analog_stored_state[b] >= analog_threshold || analog_stored_state[b] - analog_state >= analog_threshold) {
      /*int scaled_value = analog_state / analog_scale;
      scaled_value = constrain(analog_state, 0, 4095);
      scaled_value = map(scaled_value, 0, 4095, 1, 255);*/
      analog_stored_state[b] = analog_state;
      // Tell processloop that there where changes.
      faderChanges = 1;
    }
  }
  int intensity1 = analog_stored_state[9];
  int intensity2 = analog_stored_state[5];
  int intensity3 = analog_stored_state[3];
  r1 = WLEDgamma[analog_stored_state[12] * intensity1 / 65536];
  g1 = WLEDgamma[analog_stored_state[11] * intensity1 / 65536];
  b1 = WLEDgamma[analog_stored_state[10] * intensity1 / 65536];
  r2 = WLEDgamma[analog_stored_state[8] * intensity2 / 65536];
  g2 = WLEDgamma[analog_stored_state[7] * intensity2 / 65536];
  b2 = WLEDgamma[analog_stored_state[6] * intensity2 / 65536];
  r3 = WLEDgamma[analog_stored_state[0] * intensity3 / 65536];
  g3 = WLEDgamma[analog_stored_state[1] * intensity3 / 65536];
  b3 = WLEDgamma[analog_stored_state[2] * intensity3 / 65536];
}

void processInputs() {
  // Sample all analog inputs and compare to stored averages.
  readFaders();
  // Check interrupt state of SX1509 and process inputs when toggled
  // This variable is changed in the Interrupt service routine
  if (buttonChanges) {
    // Transmit current parameters to device.
    if (io.digitalRead(SX1509_GA) == 0) {
      if (WiFi.status() == WL_CONNECTED) {
        // Set the device parameters.
        // First display state on screen
        displayCommState("GO.  ", 2);
        // Serialize the data and put in the appropriate variable.
        serializeInput();
        // Create an HTTP client to do the remote work (maybe reuse possible?)
        WiFiClient client;
        HTTPClient http;
        http.begin(client, serverName1 + deviceIPs[currentDevice] + serverName2);
        // simple in CURL: curl -X POST "http://[WLED-IP]/json/state" -d '{"on":"t","bri":25}' -H "Content-Type: application/json"
        // colors in CURL: curl -X POST "http://[WLED-IP]/json/state" -d '{"seg":[{"col":[[255,170,0],[0,255,0],[64,64,64]]}]}' -H "Content-Type: application/json"
        http.addHeader("Content-Type", "application/json");

        // Workaround, WLED doesn't accept brightness 0, it wants you to turn it off.
        // modifying the stored state here has that effect (hacky, I know).
        if (analog_stored_state[3] <= 16) {
          analog_stored_state[3] = 16;
        }
        int httpResponseCode;
        Serial.println(serializedJson);
        
        sentMillis = currentMillis;
        httpResponseCode = http.POST(serializedJson);

        receivedMillis = currentMillis - sentMillis;

        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        char placeholder[4];
        itoa(httpResponseCode, placeholder, 10);

        displayCommState(placeholder);

        // Free resources
        http.end();
      } else {
        displayCommState("no wifi");
      }
    }
    // Moving right on joystick (left, because mounted upside down) gets info from the current
    // device and prints it on the serial port.
    // ToDo: also show some details on screen when in settings mode
    if (io.digitalRead(SX1509_JRIGHT) == 0) {
      // Request state from device.
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String serverPath = serverName1 + deviceIPs[currentDevice] + serverName2;

        http.begin(serverPath.c_str());

        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);
          String payload = http.getString();
          Serial.println(payload);
          char placeholder[4];
          itoa(httpResponseCode, placeholder, 10);
          displayCommState(placeholder);
        } else {
          Serial.print("Error code: ");
          Serial.println(httpResponseCode);
        }
        // Free resources
        http.end();
      }
    }
    // Moving the joystick right (joystick is mounted upside down) simply blinks the backlight.
    if (io.digitalRead(SX1509_JLEFT) == 0) {
      digitalWrite(backlightpin, 0);
      delay(200);
      digitalWrite(backlightpin, 1);
      pixels.begin();
    }
    // Moving the joystick down, next effect, this will become the rotary.
    if (io.digitalRead(SX1509_JUP) == 0) {
      // For effect mode only:
      if (interfaceMode == INTERFACEEFFECTS) {
        currentEffect++;
        if (currentEffect >= (sizeof(effectList) / 4)) {
          currentEffect = 0;
        }
        displayEffects();
      }
    }
    // Moving the joystick up, previous effect, this will become the rotary.
    if (io.digitalRead(SX1509_JDOWN) == 0) {
      // For effect mode only:
      if (interfaceMode == INTERFACEEFFECTS) {
        currentEffect--;
        if (currentEffect <= 0) {
          currentEffect = (sizeof(effectList) / 4)-1;
        }
        displayEffects();
      }
    }
    // Top white button toggles power for device.
    if (io.digitalRead(SX1509_WTOP) == 0) {
      power = !power;
      displayDeviceState();
    }
    // The lower white button selects the next device.
    if (io.digitalRead(SX1509_WBOTTOM) == 0) {
      currentDevice++;
      // Don't know why this needs to be divided by 4, maybe because of int vs byte sizes?
      if (currentDevice >= (sizeof(deviceNames) / 4)) {
        currentDevice = 0;
      }
      displayHeader();
    }
    // Pressing the encoder selects the settings page (currently analog values)
    if (io.digitalRead(SX1509_ENCODER) == 0) {
      if (interfaceMode == INTERFACEEFFECTS) {
        // Now it becomes settings
        interfaceMode = INTERFACESETTINGS;
        displaySettings(1);
      } else {
        // else it becomes effects
        interfaceMode = INTERFACEEFFECTS;
        displayEffects();
      }
    }
  }

  // Update local leds after fader changes
  if (faderChanges) {
    pixels.setPixelColor(0, pixels.Color(r3, g3, b3));
    pixels.setPixelColor(1, pixels.Color(r2, g2, b2));
    pixels.setPixelColor(2, pixels.Color(r1, g1, b1));
    pixels.show();
  }

  // If changes, update screen
  if (buttonChanges || encoderChanges || faderChanges) {
    buttonChanges = 0;
    encoderChanges = 0;
    faderChanges = 0;
    if (interfaceMode == INTERFACESETTINGS) {
      displaySettings(0);
    }
  }
}

// Short and sweet main loop, all actions are currently input reactive.
void loop() {
  // Runs currently at 10 Hz
  currentMillis = millis();
  if ((unsigned long)(currentMillis - previousMillis) >= millisBreak) {
    previousMillis = currentMillis;
    processInputs();
  }
  /*  if ((unsigned long)(currentMillis - previousHeartbeat) >= heartbeatBreak) {
    previousHeartbeat = currentMillis;
    heartBeatStatus = !heartBeatStatus;
    io.digitalWrite(SX1509_WHITELED2, heartBeatStatus);
  }*/
}
