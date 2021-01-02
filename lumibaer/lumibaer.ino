/**
 * Lumibaer
 * 
 * Copyright (C) 2021 Jens Scheidtmann (jscheidtmann)
 * This software is free software and available under a "Modified BSD license", see LICENSE.
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
 * Translate the pin 
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

#include <NanoESP.h>
#include <NanoESP_HTTP.h>

/**
 * define two constants: 
 * SSID 
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

// Change must be detected this many milliseconds at least.
#define SWITCH_THRESHOLD 50 

// Long press must be at least this long
// (at least two times SWITCH_THRESHOLD, see buttonDownDetected() )
#define SWITCH_LONGTHRESHOLD 1000

#define SINGLE_INCREMENT 51

// Globals -----------------------------------------------------------------

/**
 * Variable to enable debug output on the serial monitor.
 */
boolean debug_out = true;

/**
 * last time, a state change was detected
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
 */
uint32_t single_color = 0;

/**
 * Is it on or off?
 */
boolean single_state = false;

/**
 * Which color is selected for Single color mode?
 */
uint16_t single_hue = 0;

enum modes {
  MODE_SINGLE_COLOR, 
  MODE_SINGLE_COLOR_SELECT, 
  MODE_SINGLE_COLOR_BRIGHTNESS,
  MODE_TWO_COLOR,
  MODE_SWEEP, 
  MODE_LIGHTHOUSE,
  MODE_UNKNOWN
} lumibaer_mode = MODE_SINGLE_COLOR;

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

  // Setup Serial
  if (debug_out) {
    // Serial.begin(9600);
    Serial.begin(19200); 
  }

  boolean setup_ok = true;
  
  // Setup ESP
  nanoesp.init();

  //Connect to WiFi, start WebServer.
  setup_ok = nanoesp.configWifi(STATION, SSID, PASSWORD);
  if (setup_ok) {
    setup_ok = nanoesp.startTcpServer(80);
  }

  // Indicate state of setup 
  if (setup_ok) { // If successful, blink ...
    colorFlash(strip.Color(0, 255, 0), 200); // Green
    debug(nanoesp.getIp());
    debug("Setup: Ok");
  } else {
    colorFlash(strip.Color(255, 0, 0), 200); // Red
    debug("Setup: FAILED.");
  }
}

// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
  unsigned long now = millis();

  // Handle Button Presses (Switches to MODE_SINGLE_COLOR)
  
  if (buttonDownDetected(now)) {
    // Handle button press (on/off)
    lumibaer_mode = MODE_SINGLE_COLOR;
    single_state = !single_state; // toggle on/off
    if (single_state) {
      setSingleColor(single_color);
    } else {
      setSingleColor(strip.Color(0,0,0)); // off
    }
    strip.show();
  }
  
  if (longPressDetected(now)) {
    // On Long Press, change color.
    lumibaer_mode = MODE_SINGLE_COLOR;
    single_color = strip.gamma32(strip.ColorHSV(single_hue));
    single_hue += SINGLE_INCREMENT;
    setSingleColor(single_color);
    strip.show();
    delay(10);
  } 

  // Receive a http get request.
  /*
  String method, ressource, parameter;
  int id;

  if (http.recvRequest(id, method, ressource, parameter)) { 
    //Incoming request, parameters by reference
    
    debug("New Request from id :" + String(id) + ", method: " + method +  ", ressource: " + ressource +  " parameter: " + parameter);

    String webpage = "<h1>Hello World!";
    nanoesp.sendData(id, webpage);
  }
  */
  onboardLedSwitch();
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

boolean longPressDetected(unsigned long now) {
  if (!single_state) {
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
        debug("First false");
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
 * Set all pixels to the same color.
 */
void setSingleColor(uint32_t color) {
  for (int i=0; i <strip.numPixels(); i++) {
    strip.setPixelColor(i, color); 
  }
}

/**
 * Flash all the Pixels once
 */
void colorFlash(uint32_t color, int wait) {
  setSingleColor(color);
  strip.show();
  delay(wait);
  setSingleColor(strip.Color(0,0,0)); // off
  strip.show();
}


// Some functions of our own for creating animated effects -----------------

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}

void oneColorChase(uint32_t color, int wait) {
  for(int i=0; i < LED_COUNT; i++) {
    strip.clear();
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
  strip.clear();
  strip.show();
}
