/**
 * Lumibaer
 * 
 * Copyright (C) 2021 Jens Scheidtmann (jscheidtmann)
 * This software is free software and available under a "Modified BSD license" (3-Clause BSD), see LICENSE.
 * 
 * 
 */

// Blinkenlights ---------------------------------------------

#include <Adafruit_NeoPixel.h>

// Which pin on the Arduino is connected to the NeoPixels?
#define LED_PIN    6

// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 32

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

/**
 * Translate pin 
 */
int pinTranslate(int pin) {
  const int translation [] = { 
    // Front Ring, starting from 12 o'clock position, clockwise
    29, 28, 27, 26, 25, 24, 23, 22,  
    21, 20, 19, 18, 17, 16, 31, 30,
    // Back Ring, starting from 12 o'clock position, counter clockwise
    14, 15,  0,  1,  2,  3,  4,  5,
     6,  7,  8,  9, 10, 11, 12, 13
  };

  return translation[pin];
}


// ESP WiFi / Server Lib ---------------------------------------------------

// This is the library supplied by F. Kainka with the Pretzel Board. It is rather old and HTTP is rather limited.
#include <NanoESP.h>
#include <NanoESP_HTTP.h>

// This is the library distributed via 

/**
 * define two string constants: 
 * WIFI = SSID of your router 
 * PASSWORD
 */
#include "credentials.h"

NanoESP nanoesp = NanoESP();
NanoESP_HTTP http = NanoESP_HTTP(nanoesp); 

// LED & Switch ------------------------------------------------------------

// Standard onboard LED on an Arduino Nano
#define ONBOARD_LED 13

// The switch button is connected to which pin?
#define SWITCH 7

// Switch Press must be detected this many milliseconds at least, to count as press.
#define SWITCH_THRESHOLD 50 

// Long press must be at least this long
// (should be at least two times SWITCH_THRESHOLD, see buttonDownDetected() )
#define SWITCH_LONGTHRESHOLD 2000

// Increment applied, when changing colors. 
// This is a prime on purpose.
#define SINGLE_INCREMENT 51

// Globals -----------------------------------------------------------------

/**
 * Variable to enable debug output on the serial monitor.
 */
boolean debug_out = true;

/**
 * last time, a button down was detected
 * 
 * This variable is used to detect user interactions (presses of the button)
 */
unsigned long last_time = 0L;

/**
 * State of the button
 * 
 * T if button is pressed, F otherwise. 
 */
boolean button_state = false;

/**
 * Color that is displayed in single mode
 * 
 * Use strip.Color(R,G,B) or relatives
 */
uint32_t single_color = 0;

uint32_t front_color = 0;
uint32_t back_color = 0;

/**
 * Which color is selected for Single color mode?
 * 
 * when running through colors, ie. button is pressed. long.
 */
uint16_t single_hue = 0;

/**
 * The different modes supported by the Lumibaer software.
 */
enum modes {
  MODE_SINGLE_COLOR, // Show one color perpetually
  MODE_TWO_COLOR,  // Show different color on front and back.
  MODE_SWEEP, 
  MODE_ROTATE_TWO,
  MODE_LIGHTHOUSE,
  MODE_UNKNOWN
} lumibaer_mode = MODE_SINGLE_COLOR;

/**
 * Is Lumibär on or off?
 * 
 * For ALL modes
 */
boolean lumibaer_state = false;

// Network state -----------------------------------------------------------

boolean wifi_check_enabled = true;

boolean wifi_available = false;

boolean udp_server_running = false;

// setup() function -- runs once at startup --------------------------------

void setup() {
  // Setup digital pins
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, false);
  pinMode(SWITCH, INPUT_PULLUP); // Has inverted logic (low = button pressed)

  // Setup Neopixel strip
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(255); // Set BRIGHTNESS (max = 255), is applied during strip.show()

  // Set Default Color
  single_color = strip.Color(255,255,255); // White
  lumibaer_mode = MODE_SINGLE_COLOR;

  // Setup Serial
  if (debug_out) {
    // Serial.begin(9600);
    Serial.begin(19200); 
  }

  boolean setup_ok = true;
  
  // Setup ESP
  setup_ok = setup_ok && nanoesp.init();

  // Per default the ESP will connect to the WiFi once configured. 
  // It will take some time though. So setting it up is postponed to the loop.
  // In order to give an "immediately ready" feeling for the users.
  //
  // I tried waiting up to two seconds here, but it seems it can take longer.   

  // Indicate state of setup 
  if (setup_ok) { // If successful, blink ...
    colorFlash(strip.Color(0, 0, 255), 200); // Blue is ok (for color blind)
    debug(F("Setup: Ok"));
  } else {
    colorFlash(strip.Color(255, 0, 0), 200); // Red is bad
    debug(F("Setup: FAILED."));
  }
}

// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
  unsigned long now = millis();

  // Handle Button Presses (Switches to MODE_SINGLE_COLOR)
  
  if (buttonDownDetected(now)) {
    // Handle button press (on/off)
    lumibaer_mode = MODE_SINGLE_COLOR;
    lumibaer_state = !lumibaer_state; // toggle on/off
    synchronizeStrip();
  }
  
  if (longPressDetected(now)) {
    // On Long Press, change color (can only be true, if lumibaer is on)
    lumibaer_mode = MODE_SINGLE_COLOR;
    single_hue += SINGLE_INCREMENT;
    single_color = strip.gamma32(strip.ColorHSV(single_hue));
    synchronizeStrip();
    delay(10);
  } 

  // Make onboard LED reflect button state.
  onboardLedSwitch();

  // Try setting up the WiFi (once a second)
  static unsigned long last_check = 0L;
  static int wifi_check_count = 0;
  
  if (wifi_check_enabled && (now - last_check) > 1000L) {
    wifi_check_count++;
    String response = nanoesp.getIp();
    if (-1 == response.indexOf("0.0.0.0")) {
      wifi_check_enabled = false;
      wifi_available = true;
      debug(response);
      debug(String(wifi_check_count));
      colorFlash(strip.Color(0, 255, 0), 50); // Green
    } else {
      last_check = now;
      if (wifi_check_count > 10) {
        colorFlash(strip.Color(0,255,255), 50); // yellow
        // connect to WiFi
        nanoesp.configWifi(STATION, WIFI, PASSWORD);
        // Enable Autoconnection (saves to ESP flash). 
        // Once done, it should auto set up, during the 10 seconds wait time,
        // so we never get here again.
        nanoesp.sendCom("AT+CWAUTOCONN=1"); 
        wifi_check_count = 0;
      } else {
        colorFlash(strip.Color(255,0,0), 50); // Red
      }
    }
  }

  if (wifi_available && !udp_server_running) {
    if (nanoesp.startUdpServer(0,"192.168.178.255", 8888,8888,2)) {
      udp_server_running = true;
      debug(F("UDP server started."));
    } else {
      debug(F("FAILURE starting UDP server."));
    }
  }

  // / *
  int client, len; 
  while (nanoesp.recvData(client, len)) {
    String request = nanoesp.readString();
    debug("recv:"); debug(request);
    if (request.startsWith(F(":on"))) {
      lumibaer_state = true;
      debug(F("Set ON!"));
    } else if (request.startsWith(F(":off"))) {
      lumibaer_state = false;
      debug(F("Set OFF!"));
    } else if (request.startsWith(F(":brightness?"))) {
      debug(F("brightness"));
      int brightness = strtol(request.substring(12).c_str(), NULL, 10);
      if (brightness < 0) brightness = 0;
      if (brightness > 255) brightness = 255;
      strip.setBrightness(brightness);
    } else if (request.startsWith(F(":toggle"))) {
      lumibaer_state = !lumibaer_state;
      debug(F("Toggle!"));
    } else if (request.startsWith(F(":color?"))) {
      lumibaer_mode = MODE_SINGLE_COLOR;
      debug(request.substring(7,13));
      uint32_t color = strtoul(request.substring(7,13).c_str(),NULL, 16);
      lumibaer_state = true;
      single_color = color;
      debug(F("Single color set"));
    } else if (request.startsWith(F(":two?"))) {
      lumibaer_mode = MODE_TWO_COLOR;
      lumibaer_state = true;
      debug(F("TWO"));
      debug(request.substring(5,11));
      debug(request.substring(12,18));
      front_color = strtoul(request.substring(5, 11).c_str(), NULL, 16);
      back_color = strtoul(request.substring(12,18).c_str(), NULL, 16);
    }
    synchronizeStrip();
  }
  
  // */
  /*
  // Serial Monitor passes stuff through to the ESP.
  while (Serial.available()) {
    nanoesp.write(Serial.read());
    delay(10);
  } 
  while (nanoesp.available()) {
    Serial.write(nanoesp.read());
  } 
  */
}

// State bearing functions -------------------------------------------------

/**
 * Detect a button down event               
 * 
 *                                           detect!
 *                         trigger          /        not detected
 *                        /                /        /
 * Button pressed:        |-----| |-------|----|   |---------| 
 *                        |     | |            |   |         |
 * Button not pressed: ---|     |-|       |    |---|         |-------- 
 *                                        |
 *                        |<- threshold ->|<- threshold ->|
 *      |-- active -->        wait time       dead time   |-- active -->
 *  
 * - only detect a press (button down) not a release. 
 * - recheck, that button is still down, when threshold has passed.
 * - then do not detect another change for dead time (3x threshold)
 * 
 */
boolean buttonDownDetected(unsigned long now) {
  static unsigned long last_changetime = 0L;
  static boolean last_state = false;
  static boolean dead_time = false;

  // if a last_changetime is set, ... 
  if (last_changetime != 0L && (now - last_changetime) < SWITCH_THRESHOLD) {
    // ... wait time or dead time are running, return immediately
    return false;
  } 
  
  // Read current button state.
  boolean val = not digitalRead(SWITCH);

  if (last_changetime != 0L) {
    // wait time or dead time were running, but it expired...
    if (dead_time) {
      last_changetime = 0L;
      dead_time = false;
    } else {
      // We have run through threshold, then start dead time
      last_changetime = now; 
      dead_time = true;
      debug(F("start dead time"));
      return val;
    } 
  }

  // Did button state change?
  if (val != last_state) {
    // Yes, save state.
    last_state = val;
    // Trigger wait time on press.
    if (val) {
      // Start wait time.
      last_changetime = now;
    }
  }
  return false;
}

/**
 * Detect a long press of the button
 * 
 * Returns: 
 * - false, if Lumibär is off,
 * - true, if the button was pressed at beginning and end of a SWITCH_LONGTHRESHOLD period, 
 * - false otherwise.
 * 
 * A button press triggers a wait period, that returns false always. 
 * If the button is (still) pressed at the end of the wait, returns true.
 */
boolean longPressDetected(unsigned long now) {
  if (!lumibaer_state) {
    return false;
  }
  
  static unsigned long last_time = 0L;
  static boolean first = true;
  static boolean last_state = false;

  if (last_time != 0L && (now - last_time) < SWITCH_LONGTHRESHOLD) {
    return false;
  }
  
  // Read current button state.
  boolean val = not digitalRead(SWITCH);
  if (val != last_state) {
    // Button state changed.
    last_state = val;
    if (val) {
      if (first) { // Trigger Threshold
        last_time = now;
        first = false;
        // debug("First false");
        return false;
      }
    } else {
      last_time = 0L;
      first = true;
      // debug("First true");
    }
  }
  return val;
}

// Helper functions --------------------------------------------------------
/**
 * Display a debug message on the Serial Monitor
 */
void debug(String msg) {
  if (debug_out) {
    Serial.println(msg);
  }
}

/**
 * Read switch button state, reflect it in the onboard LED.
 * 
 * If the state changed, display it in the Serial Monitor.
 * Does not change any of the state variables. 
 */
void onboardLedSwitch() {
  static boolean old_state = false;
  boolean val = digitalRead(SWITCH);
  digitalWrite(ONBOARD_LED, !val);

  if (debug_out && val != old_state) {
    if (!val) {
      debug("Button pressed!");
    } else {
      debug("Button released!");
    }
    old_state = val;
  }
}

// Functions dealing with the color strip ----------------------------------

/**
 * Make strip reflect the internal lumibaer_state.
 */
void synchronizeStrip() {
  debug(F("synchronizeStrip"));
  debug(String(lumibaer_mode));
  if (lumibaer_state) {
    switch (lumibaer_mode) {
      case MODE_SINGLE_COLOR: 
        debug("Single");
        debug(String(single_color));
        setSingleColor(single_color); 
        break;
      case MODE_TWO_COLOR:
        debug("Two");
        debug(String(front_color, 16));
        debug(String(back_color, 16)); 
        setTwoColors(front_color, back_color); 
        break;
      default: 
        debug("other");
        setSingleColor(strip.Color(255,0,128));
        // TODO
        break;
    }
  } else {
    setSingleColor(strip.Color(0,0,0)); // off
  }
  debug("strip.show");
  strip.show();
}


/**
 * Set all pixels to the same color.
 */
void setSingleColor(uint32_t color) {
  for (int i=0; i <strip.numPixels(); i++) {
    strip.setPixelColor(i, color); 
  }
}

/**
 * 
 */
void setTwoColors(uint32_t front, uint32_t back) {
  for (int i = 0; i < LED_COUNT/2; i++) {
    strip.setPixelColor(pinTranslate(i), front);  
  }
  for (int i = LED_COUNT/2; i < LED_COUNT; i++) {
    strip.setPixelColor(pinTranslate(i), back);
  }
}

/**
 * Flash all the Pixels once
 * 
 * Retrieves current state and restores it after the flash.
 * Takes 3x wait time, as it is: save, off, flash, off, restore.
 */
void colorFlash(uint32_t color, int wait) {
  // Save current state
  uint16_t cache[LED_COUNT];
  for (int i = 0; i < LED_COUNT; i++) {
    cache[i] = strip.getPixelColor(i);
  }
  setSingleColor(strip.Color(0,0,0)); // off
  strip.show();
  delay(wait);
  setSingleColor(color);
  strip.show();
  delay(wait);
  setSingleColor(strip.Color(0,0,0)); // off
  strip.show();
  delay(wait);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, cache[i]);
  }
}
 
