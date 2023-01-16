#include <HTTPClient.h>

/*
 * esp32 v 2.0.5 seems to have an issue with the SPI screen
 * neopixel seems to require something more recent than 2.0.1
 * 
 */
#include <Freenove_WS2812_Lib_for_ESP32.h>

#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include <WiFi.h>

#include <Wire.h>           // Include the I2C library (required)
#include <SparkFunSX1509.h> //Click here for the library: http://librarymanager/All#SparkFun_SX1509

#include "heart.h"
#include "smiley.h"
#include "gear.h"
#include "pallete.h"

const byte SX1509_ADDRESS = 0x3E; // SX1509 I2C address
#define sdaPin 26
#define sclPin 22
const byte SX1509_WHITELED1 = 4; // LED connected to 4 (draining current)
const byte SX1509_WHITELED2 = 5; // LED connected to 5 (draining current)
const byte SX1509_JUP       = 15; // Directional pad
const byte SX1509_JDOWN     = 14; // Directional pad
const byte SX1509_JLEFT     = 13; // Directional pad
const byte SX1509_JRIGHT    = 12; // Directional pad
const byte SX1509_JMID      = 11; // Directional pad
const byte SX1509_GA        =  6; // DBig blue button
const byte SX1509_4067_S0   =  0; // 4067 analog multiplexer
const byte SX1509_4067_S1   =  1; // 4067 analog multiplexer
const byte SX1509_4067_S2   =  2; // 4067 analog multiplexer
const byte SX1509_4067_S3   =  3; // 4067 analog multiplexer

const byte SX1509_INTERRUPT_PIN = 27;


// For preview leds:
#define NUMPIXELS 3
#define LEDS_PIN 33
#define CHANNEL    0
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(NUMPIXELS, LEDS_PIN, CHANNEL);

SX1509 io;                        // Create an SX1509 object to be used throughout

#define backlightpin 4 // LCD backlight

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

// Color is 16-bit, and that includes Red, Green and Blue in a 16-bit variable. 
// The way the color is packed in is the top 5 bits are red, the middle 6 bits are green and the bottom 5 bits are blue.

#define TFT_MYGREY   0x5AEB
#define TFT_MYBACK   0x1042
#define TFT_MYACCENT 0x30C6
#define TFT_MYBORDER 0x514A
#define TFT_MYORANGE 0xFC80

#define ACCENTED_COLORS 1
#define ACCENTED_EFFECTS 2
#define ACCENTED_FAVORITES 3
#define ACCENTED_SETTINGS 4

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long millisBreak = 100;
unsigned long previousHeartbeat = 0;
unsigned long heartbeatBreak = 1000;

byte buttonChanges = 1;
byte encoderChanges = 1;
byte faderChanges = 1;
byte status = 0;

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

char const *deviceNames[] = {"Stok",
                       "Decoratie",
                       "Bank",
                       "Jongens",
                       "Kerstboom"};
               
char const *deviceIPs[] = {"43",
                     "47",
                     "1",
                     "137",
                     "196"};

byte currentDevice = 0;

bool wifi = 0;

// Local values to be sent remote
byte brightness = 0;
bool power = 0;
bool showSettings = 0;

// For the heartbeat led
bool heartBeatStatus = 1;

void displayDraw() {
  tft.fillScreen(TFT_MYBACK);
  // Header
  tft.fillRect(0,0,239,36, TFT_MYBORDER);
  // Side bar (excluding top part)
  tft.fillRect(0,36,47,319, TFT_MYBORDER);
}

void displayOn() {
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_RED);  
  tft.setCursor(170, 5);
  tft.print("on");
}

inline void displayCommState(char *message) {
  tft.setCursor(5, 85);
  tft.print(message);
}

void displayDeviceState() {
  // Redraw state of device, on or off.
  tft.setCursor(5,55);
  tft.setTextSize(2);
  tft.setTextColor(TFT_MYORANGE, TFT_MYBORDER);
  if (power) {
    tft.print(" on");
  } else {
    tft.print("off");
  }
}

void displayIcons(int accented) {
  // Redraw side bar (excluding top part)
  tft.fillRect(0,100,47,319, TFT_MYBORDER);

  // Now draw icons, accented is the selected 'page'.
  if (accented == 1) {
    tft.fillRect(5,179+((accented-1)*31),37,33, TFT_MYACCENT);
  }
  if (accented == 2) {
    tft.fillRect(5,214,37,33, TFT_MYACCENT);
  }
  if (accented == 3) {
    tft.fillRect(5,249,37,33, TFT_MYACCENT);
  }
  if (accented == 4) {
    tft.fillRect(5,281,37,31, TFT_MYACCENT);
  }
  tft.pushImage(8,180,31,31,palleteimg, 0x00); // colors
  tft.pushImage(8,215,30,31,smiley, 0x00); // effects
  tft.pushImage(8,250,31,27,heart, 0x00); // favorites
  tft.pushImage(9,282,29,29,gear, 0x00); // settings / debug
}

void displayEffects() {
  displayIcons(ACCENTED_EFFECTS);
  tft.setViewport(55, 43, 184, 276);
  tft.setTextColor(TFT_MYORANGE, TFT_MYBACK);
  tft.fillScreen(TFT_MYBACK);  
  tft.setCursor(0,0);
  tft.println("Solid");
  tft.println("Blink");
  tft.println("Breathe");
  tft.println("Wipe");
  tft.println("Wipe Random");
  tft.println("Random Colors");
  tft.println("Sweep");
  tft.println("Dynamic");
  tft.println("Colorloop");
  tft.println("Rainbow");
  tft.println("Scan");
  tft.println("Scan Dual");
  tft.println("Fade");
  tft.println("Theater");
  tft.println("Theater Rainbow");
  tft.println("Running");
  tft.resetViewport();
}

void displayHeader() {
  // This also clears the wifi state display, not that that was ever updated.
  tft.fillRect(0,0,239,36, TFT_MYBORDER);
  tft.setCursor(5,5);
  tft.setTextSize(4);
  tft.setTextColor(TFT_MYORANGE, TFT_MYBORDER);
  tft.print(deviceNames[currentDevice]);
  tft.setTextSize(2);
}

void displaySettings(bool clearBack) {
  displayIcons(ACCENTED_SETTINGS);
  tft.setViewport(55, 43, 184, 276);
  if (clearBack) {
    tft.fillScreen(TFT_MYBACK);  
  }
  tft.setCursor(10, 10);
  tft.printf("%03d",analog_stored_state[3]/16);
  tft.setCursor(10, 30);
  tft.printf("%03d",analog_stored_state[2]/16);
  tft.setCursor(10, 50);
  tft.printf("%03d",analog_stored_state[1]/16);
  tft.setCursor(10, 70);
  tft.printf("%03d",analog_stored_state[0]/16);
  tft.setCursor(10, 90);
  tft.printf("%03d",analog_stored_state[4]/16);
  tft.setCursor(10,200);
  tft.print(io.digitalRead(SX1509_JUP));
  tft.print(io.digitalRead(SX1509_JDOWN));
  tft.print(io.digitalRead(SX1509_JLEFT));
  tft.print(io.digitalRead(SX1509_JRIGHT));
  tft.print(io.digitalRead(SX1509_JMID));
  tft.resetViewport();
}

void multiplexerSetup() {
  // The SX1509 interrupt is active-low.
  pinMode(SX1509_INTERRUPT_PIN, INPUT_PULLUP);
  
  Wire.begin(sdaPin, sclPin);

  // Call io.begin(<address>) to initialize the SX1509. If it
  // successfully communicates, it'll return 1.
  if (io.begin(SX1509_ADDRESS) == false)
  {
    Serial.println("Failed to communicate. Check wiring and address of SX1509.");
    digitalWrite(22, HIGH); // If we failed to communicate, turn the pin 13 LED on
    while (1)
      ; // If we fail to communicate, loop forever.
  }

  io.debounceTime(20); // Set debounce time to 32 ms.
  
  io.pinMode(SX1509_WHITELED1, OUTPUT);
  io.pinMode(SX1509_WHITELED2, OUTPUT);
  io.pinMode(SX1509_JUP,       INPUT_PULLUP);
  io.pinMode(SX1509_JDOWN,     INPUT_PULLUP);
  io.pinMode(SX1509_JLEFT,     INPUT_PULLUP);
  io.pinMode(SX1509_JRIGHT,    INPUT_PULLUP);
  io.pinMode(SX1509_JMID,      INPUT_PULLUP);
  io.pinMode(SX1509_GA,        INPUT_PULLUP);
  io.pinMode(SX1509_4067_S0,   OUTPUT);
  io.pinMode(SX1509_4067_S1,   OUTPUT);
  io.pinMode(SX1509_4067_S2,   OUTPUT);
  io.pinMode(SX1509_4067_S3,   OUTPUT);
  
  io.enableInterrupt(SX1509_JUP, CHANGE);
  io.enableInterrupt(SX1509_JDOWN, CHANGE);
  io.enableInterrupt(SX1509_JLEFT, CHANGE);
  io.enableInterrupt(SX1509_JRIGHT, CHANGE);
  io.enableInterrupt(SX1509_JMID, CHANGE);
  io.enableInterrupt(SX1509_GA, CHANGE);

  io.debouncePin(SX1509_JUP); // Enable debounce
  io.debouncePin(SX1509_JDOWN); // Enable debounce
  io.debouncePin(SX1509_JLEFT); // Enable debounce
  io.debouncePin(SX1509_JRIGHT); // Enable debounce
  io.debouncePin(SX1509_JMID); // Enable debounce
  io.debouncePin(SX1509_GA); // Enable debounce

  attachInterrupt(digitalPinToInterrupt(SX1509_INTERRUPT_PIN), processButtons, FALLING);
  
  // Blink the LED a few times before we start:
  for (int i = 0; i < 2; i++)
  {
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

  
  Serial.begin(115200);
  Serial.println("Herman was here... ");
  Serial.println(millis());

  multiplexerSetup();
  
  // Display stuff
  tft.begin();
  tft.setRotation(0);

  displayDraw();
  displayOn();
  Serial.println(millis());
  
  // Turn on Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin("Lief delen, niet stelen.", "gewoondoen");
  Serial.print("Connecting to WiFi ..");
  tft.setCursor(170, 5);
  for (int i = 0; i < 5; i++) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      tft.print('.');
      delay(1000);
      wifi = 0;
    } else {
      wifi = 1;
      Serial.println(WiFi.localIP());
      tft.setCursor(170, 5);
      tft.print("wifi");
      break;
    }
  }

  strip.begin();
    
  Serial.println(millis());
  displayEffects();
  displayHeader();
  Serial.println(deviceNames[currentDevice]);
  Serial.println(millis());
}

void readFaders() {
  // Borrowed from earlier work on musicmonster
  // maybe add some running averaging? If fast enough
  for (b = 0; b < analogConnected; b++) {
    // select the bit  
    io.digitalWrite(SX1509_4067_S0, bitRead(b,0));
    io.digitalWrite(SX1509_4067_S1, bitRead(b,1));
    io.digitalWrite(SX1509_4067_S2, bitRead(b,2));
    io.digitalWrite(SX1509_4067_S3, bitRead(b,3));

    analog_state = analogRead(analog4067pin);    
    
     // This voodoo malarky needs some documentation.
    if (analog_state - analog_stored_state[b] >= analog_threshold || analog_stored_state[b] - analog_state >= analog_threshold) {
      int scaled_value = analog_state / analog_scale;
      scaled_value = constrain(analog_state, 0, 4095);
      scaled_value = map(scaled_value, 0, 4095, 1, 255);          
      
      analog_stored_state[b] = analog_state;
      // Tell processloop that there where changes.
      faderChanges = 1;
    }
  }
}

void processInputs() {
  // Sample all analog inputs and compare to stored averages.
  readFaders();
  // Check interrupt state of SX1509 and process inputs when toggled
  if (buttonChanges) {
    if (io.digitalRead(SX1509_JUP)==0) {
      power = !power;
      displayDeviceState();
    }
    if (io.digitalRead(SX1509_GA) == 0) {
      if(WiFi.status()== WL_CONNECTED){
        // Set the device parameters.
        // First display state on screen
        displayCommState("go.");
        // Send the two parameters.
        WiFiClient client;
        HTTPClient http;
        http.begin(client, serverName1 + deviceIPs[currentDevice] + serverName2);
        // in CURL: curl -X POST "http://[WLED-IP]/json/state" -d '{"on":"t","bri":25}' -H "Content-Type: application/json"
       
        String json1 = "{\"on\":";
        String json2 = ",\"bri\":";
        String json3 = "}";
        String pwrjsontrue = "true";
        String pwrjsonfalse = "false";
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode;
        //httpResponseCode = http.POST("{\"on\":false,\"bri\":25}");
        if (power) {
          httpResponseCode = http.POST(json1 + pwrjsontrue + json2 + analog_stored_state[0]/16 + json3);  
          Serial.println(json1 + pwrjsontrue + json2 + analog_stored_state[0]/16 + json3);
        } else {
          httpResponseCode = http.POST(json1 + pwrjsonfalse + json2 + analog_stored_state[0]/16 + json3);  
          Serial.println(json1 + pwrjsonfalse + json2 + analog_stored_state[0]/16 + json3);
        } 
        
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        char placeholder[4];
        itoa(httpResponseCode, placeholder, 10);
        displayCommState(placeholder);
          
        // Free resources
        http.end();
      }
    }
    if (io.digitalRead(SX1509_JRIGHT) == 0) {
      // Request state from device.
      if(WiFi.status()== WL_CONNECTED){
        HTTPClient http;    
        String serverPath = serverName1 + deviceIPs[currentDevice] + serverName2;
          
        // Your Domain name with URL path or IP address with path
        http.begin(serverPath.c_str());
    
        int httpResponseCode = http.GET();
    
        if (httpResponseCode>0) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);
          String payload = http.getString();
          Serial.println(payload);
          tft.setCursor(170, 25);
          tft.print(httpResponseCode);
        }
        else {
          Serial.print("Error code: ");
          Serial.println(httpResponseCode);
        }
        // Free resources
        http.end();
      }
    }
    if (io.digitalRead(SX1509_JLEFT) == 0) {
      currentDevice++;
      if (currentDevice >= (sizeof(deviceNames)/4)) {
        currentDevice = 0;
      }
      displayHeader();
    }
    if (io.digitalRead(SX1509_JMID) == 0) {
      showSettings = !showSettings;
      if (showSettings) {
        displaySettings(1);
      } else {
        displayEffects();
      }
      
    }
  }
  
  // Update Rotary encoders
  if (faderChanges) {
    //pixels.fill(0xFF0000);
    //pixels.setPixelColor(0, 20, 20, 20);
    int fader = analog_stored_state[0]/16;
    fader = 256;
    int r = analog_stored_state[3]/16*(fader/256);
    int g = analog_stored_state[2]/16*(fader/256);
    int b = analog_stored_state[1]/16*(fader/256);
    strip.setLedColorData(0, g, r, b);
    strip.show();
  }
  
  // If changes, update screen
  if (buttonChanges || encoderChanges || faderChanges) {
    buttonChanges = 0;
    encoderChanges = 0;
    faderChanges = 0;
    if (showSettings){
      displaySettings(0);
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  currentMillis = millis();
  if ((unsigned long)(currentMillis - previousMillis) >= millisBreak) {
    previousMillis = currentMillis;
    processInputs();
  }
  if ((unsigned long)(currentMillis - previousHeartbeat) >= heartbeatBreak) {
    previousHeartbeat = currentMillis;
    heartBeatStatus = !heartBeatStatus;
    io.digitalWrite(SX1509_WHITELED2, heartBeatStatus);
  }
  
}
