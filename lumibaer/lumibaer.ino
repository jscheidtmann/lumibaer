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
    // Back Ring, starting from one next to 12 o'clock position, counter clockwise
    13, 14, 15,  0,  1,  2,  3,  4,  
     5,  6,  7,  8,  9, 10, 11, 12, 
  };

  return translation[pin];
}


// ESP WiFi / Server Lib ---------------------------------------------------

// This is the library supplied by F. Kainka with the Pretzel Board. It is rather old and HTTP is rather limited.
#include <NanoESP.h>
#include <NanoESP_HTTP.h>

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

// Time (ms) waited when rotating colors
// (default value)
#define ROTATE_WAIT 50

// Time (ms) for Morse wait
// Short (.) = this time, long (-) = 3x this time.
// applies also to breaks between . and -, and words (long)
#define MORSE_WAIT 300

// Default Brightness 0-255
#define DEFAULT_BRIGHTNESS 50

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

/**
 * Colors for Front/Back mode: Front color
 */
uint32_t front_color = 0;

/**
 * Colors for Front/Back mode: Back color
 */
uint32_t back_color = 0;

/**
 * Colors for ROTATE_MODE: Left color
 */
uint32_t left_color = 0;

/**
 * Colors for ROTATE_MODE: Right color
 */
uint32_t right_color = 0;

/**
 * Colors for sweep mode (back and forth): First color
 */
uint32_t sweep1 = 0;

/**
 * Colors for sweep mode (back and forth): Second color
 */
uint32_t sweep2 = 0;

/**
 * Wait this many millisecond for a rotation step
 * 
 * Is set by "rotatewait" UDP command.
 * Smaller means quicker. 
 */
unsigned int rotate_wait = ROTATE_WAIT;

/**
 * State of rotation. 
 * 
 * Will be incremented, if one of the rotation modes is active.
 * (
 */
unsigned int rotate = 0;

/**
 * Which color is selected for Single color mode?
 * 
 * when running through colors, ie. button is long pressed.
 */
uint32_t single_hue = 0;

/**
 * The different modes supported by the Lumibaer software.
 */
enum modes {
  MODE_SINGLE_COLOR, // Show one color perpetually
  MODE_TWO_COLOR,    // Show different color on front and back.
  MODE_ROTATE_TWO,   // Show two colors on halves, then rotate.
  MODE_SWEEP,        // Sweep between two colors
  MODE_LIGHTHOUSE,   // Parse and show a lighthouse Kennung
  MODE_MORSE,        // Show a Morse Code message.
  MODE_UNKNOWN
} lumibaer_mode = MODE_SINGLE_COLOR;

/**
 * Is Lumibär on or off?
 * 
 * For ALL modes
 */
boolean lumibaer_state = false;

/**
 * State for animations in modes: MODE_MORSE and MODE_LIGHTHOUSE
 */
struct animation_step {
  unsigned long wait;
  enum { 
    SINGLE,
    TWO, 
    THREE  
  } mode;
  uint32_t color;
  uint32_t color2;
  uint32_t color3;
  int next;
} animation_steps[10];

int active_step = 0;

// Network state -----------------------------------------------------------

/**
 * 
 */
boolean wifi_check_enabled = true;

boolean wifi_available = false;

boolean udp_server_running = false;

// setup() function -- runs once at startup --------------------------------

/**
 * Setup the Lumibaer
 * 
 * 1. Setup digital pins
 *     ONBOARD_LED: output, off
 *     SWITCH: input pullup
 *     
 * 2. Setup Neopixel rings/strip
 *     Set brightness to 50, to avoid Lumibaer`s head to get warm
 *     
 * 3. Default Mode: White, Single color mode
 * 
 * 4. Setup Serial (19200)
 * 
 * 5. Initialize NanoESP
 *     Does not yet setup the WiFi, this is done in the Main loop.
 *     
 * 6. Blink once in Blue, if setup worked 
 *     (Setup will not fail)
 */
void setup() {
  // Setup digital pins
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, false);
  pinMode(SWITCH, INPUT_PULLUP); // Has inverted logic (low = button pressed)

  // Setup Neopixel strip
  strip.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();   // Turn OFF all pixels ASAP
  strip.setBrightness(DEFAULT_BRIGHTNESS); // Set BRIGHTNESS (max = 255), is applied during strip.show()

  // Set Default Color
  single_color = strip.Color(255,255,255); // White
  lumibaer_mode = MODE_SINGLE_COLOR;

  // Set Default Lighthouse Mode 
  // Amrum Lighthouse (north sea): Fl. 6.5s
  animation_step * p = animation_steps;
  p->mode = animation_step::SINGLE;
  p->wait = 1000;
  p->next = 1;
  p->color = strip.Color(255,255,255); // White
  p++;
  p->mode = animation_step::SINGLE;
  p->wait = 5500;
  p->next = 0;
  p->color = strip.Color(0,0,0); // Black
  
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

/**
 * Main Loop
 * 
 * All waits are done using comparisons against "now", which is initialized at the start of the loop.
 * wait() is never called.
 * 
 * This is the order in which things are done:
 *  1.) Check state of the button. 
 *       Do we have a short press? -> Switch Lumibaer on/off
 *       Do we have a long press? -> Cycle colors
 *       Synchronize the onboard LED with the button press.
 *       
 *  2.) Setup the WiFi 
 *       Connect to the WiFi.
 *          Checks after 1s, if connect was successful. 
 *          If successful, flash green, if not successful, flash red.
 *          After 10 times, the connect is retried and a yellow flash occurs.
 *       If successful start a UDP server.
 *       
 *  3.) Handle UDP commands to the Lumibaer.
 *      The routine knows the following commands:
 *       "on", "off": 
 *          Switch Lumibaer on or off remotely
 *       "toggle":
 *          If Lumibaer is on, set it to off and vice versa
 *       "brightness?<x>": 
 *          Set brightness of NeoPixel strip. 
 *          <x> a number between 0 and 255, 
 *          if x is negative the default brightness will be set.
 *       "color?ffddee":
 *          Set color for single color mode. Pass the RGB colors as HTML color code (without "#")
 *          Switches to MODE_SINGLE and the Lumibaer on.
 *       "lighthouse?Oc(3) 3s RWG":
 *          Turns on lighthouse mode. Parses a lighthouse specification into animation_steps, 
 *          see https://en.wikipedia.org/wiki/Light_characteristic 
 *       "morse?sos":
 *          Turns on Morse code mode. Parses a string into animation_steps, using Morse Code. 
 *          see https://en.wikipedia.org/wiki/Morse_code
 *       "rotate?ffddee,aabbcc":   
 *          half of the ring is color "ffddee", other half of ring is "aabbcc", as HTML color code.
 *          Switches to MODE_ROTATE_TWO and the Lumibaer on.
 *       "sweep?ffddee,aabbcc":
 *          Change color from ffddee to aabbcc, interpolating linearly for each color channel.   
 *          Siwtches to MODE_SWEEP and the Lumibaer on.
 *       "two?ffddee,aabbcc":
 *          Set color for two color mode (Front and Back different color)
 *          Pass the two colors as RGB colors (replacing ffddee and aabbcc with respective values).
 *          Switches to MODE_TWO_COLOR and the Lumibaer on.
 *          
 * 4.) Handle Animation
 *       If a mode needs animation and is active, the rotation variable is increased 
 *       and the strip state is synchronized.
 *           
 */
void loop() {
  unsigned long now = millis();

  /////////////////////////////////////////////////////////////////////////////////  
  // Step 1.)
  // Handle Button Presses (Switches to MODE_SINGLE_COLOR)
  
  if (buttonDownDetected(now)) {
    // Handle button press (on/off)
    lumibaer_state = !lumibaer_state; // toggle on/off
    synchronizeStrip();
  }
  
  if (longPressDetected(now) && lumibaer_mode == MODE_SINGLE_COLOR) {
    single_hue += SINGLE_INCREMENT;
    single_color = strip.gamma32(strip.ColorHSV(single_hue));
    synchronizeStrip();
    delay(10);
  } 

  // Make onboard LED reflect button state.
  onboardLedSwitch();

  /////////////////////////////////////////////////////////////////////////////////  
  // Step 2.)
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

  /////////////////////////////////////////////////////////////////////////////////
  // Step 3.) Serve UDP Commands
  
  // / * // Comment out Step 3, for passthrough, see below
  int client, len; 
  while (nanoesp.recvData(client, len)) {
    String request = nanoesp.readString();
    debug("recv:"); debug(request);
    if (request.startsWith(F(":on"))) {
      lumibaer_state = true;
      // debug(F("Set ON!"));
    } else if (request.startsWith(F(":off"))) {
      lumibaer_state = false;
      // debug(F("Set OFF!"));
    } else if (request.startsWith(F(":brightness?"))) {
      debug(F("brightness"));
      int brightness = strtol(request.substring(12).c_str(), NULL, 10);
      if (brightness < 0) brightness = DEFAULT_BRIGHTNESS;
      if (brightness > 255) brightness = 255;
      strip.setBrightness(brightness);
    } else if (request.startsWith(F(":rotwait?"))) {
      debug(F("rotwait"));
      rotate_wait = strtoul(request.substring(9).c_str(), NULL, 10);
      // Avoid values that are two small. 
      if (rotate_wait < 100) rotate_wait = 100;
    } else if (request.startsWith(F(":toggle"))) {
      lumibaer_state = !lumibaer_state;
      // debug(F("Toggle!"));
    } else if (request.startsWith(F(":color?"))) {
      lumibaer_mode = MODE_SINGLE_COLOR;
      // debug(request.substring(7,13));
      uint32_t color = strtoul(request.substring(7,13).c_str(),NULL, 16);
      lumibaer_state = true;
      single_color = color;
      debug(F("Single color set"));
    } else if (request.startsWith(F(":two?"))) {
      lumibaer_mode = MODE_TWO_COLOR;
      lumibaer_state = true;
      // debug(F("TWO"));
      // debug(request.substring(5,11));
      // debug(request.substring(12,18));
      front_color = strtoul(request.substring(5, 11).c_str(), NULL, 16);
      back_color = strtoul(request.substring(12,18).c_str(), NULL, 16);
    } else if (request.startsWith(F(":rotate?"))) {
      lumibaer_mode = MODE_ROTATE_TWO;
      lumibaer_state = true;
      rotate = 0;
      left_color = strtoul(request.substring(8, 14).c_str(), NULL, 16);
      right_color = strtoul(request.substring(15,21).c_str(), NULL, 16);
    } else if (request.startsWith(F(":sweep?"))) {
      lumibaer_mode = MODE_SWEEP;
      lumibaer_state = true;
      rotate=0;
      sweep1 = strtoul(request.substring(7, 13).c_str(), NULL, 16);
      sweep2 = strtoul(request.substring(14, 20).c_str(), NULL, 16);
    } else if (request.startsWith(F(":lighthouse?"))) {
      lumibaer_mode = MODE_LIGHTHOUSE;
      lumibaer_state = true;
      rotate=0;
      parseLighthouseSpec(request.substring(12));
    } else if (request.startsWith(F(":morse?"))) {
      lumibaer_mode = MODE_MORSE;
      lumibaer_state = true;
      rotate=0;
      parseMorseSpec(request.substring(7));
    }
    synchronizeStrip();
  }

  /////////////////////////////////////////////////////////////////////////////////  
  // Step 4.) Handle animations
  if (lumibaer_state) {
    static unsigned long last_rotate = 0L;
    // Lumibaer needs to be on.
    switch (lumibaer_mode) {
      case MODE_ROTATE_TWO:
      case MODE_SWEEP: 
        if (last_rotate == 0L) {
          last_rotate = now; 
        } else if ((now - last_rotate) > rotate_wait) {
          last_rotate = now;
          // debug(F("rotate"));
          rotate++;
          synchronizeStrip();
        }
        break;
      case MODE_LIGHTHOUSE:
      case MODE_MORSE:
        if (last_rotate == 0L) {
          last_rotate = now;
        } else if ((now - last_rotate) > animation_steps[active_step].wait) {
          last_rotate = now;
          synchronizeStrip();
        }
      default: 
        ; // No Op
        break;
    }    
  }
  // */ // Comment out step 3
  /*
  //////////////////////////////////////////////////////////////////////
  // Pass input from Serial through to the NanoESP. 
  // 
  // !!! To avoid clashes with UDP command handling, you need to comment out Step 3!!!
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


// Lighthouse --------------------------------------------------------------

/**
 * Oc(2)WRG.9s
 */
void parseLighthouseSpec(const String & spec) {
  
}


// Morse -------------------------------------------------------------------

/**
 * SOS = ... --- ...
 * 
 */
 void parseMorseSpec(const String & spec) {
  
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
 * 
 * This function calls strip.show() at the end. 
 * Should be called only, if the strip state needs to change.
 * Colors that are called from here, should not call strip.show() themselves.
 */
void synchronizeStrip() {
  // debug(F("synchronizeStrip"));
  // debug(String(lumibaer_mode));
  if (lumibaer_state) {
    switch (lumibaer_mode) {
      case MODE_SINGLE_COLOR: 
        debug(F("Single"));
        debug(String(single_color));
        setSingleColor(single_color); 
        break;
      case MODE_TWO_COLOR:
        debug(F("Two"));
        debug(String(front_color, 16));
        debug(String(back_color, 16)); 
        setTwoColors(front_color, back_color); 
        break;
      case MODE_ROTATE_TWO:
        // debug(F("Rotate"));
        rotateTwoColors(left_color, right_color);
        break;
      case MODE_SWEEP:
        // debug(F("sweep"));
        sweepTwoColors(sweep1, sweep2);
        break;
      case MODE_LIGHTHOUSE:
      case MODE_MORSE:
        switch(animation_steps[active_step].mode) {
          case animation_step::SINGLE:
            setSingleColor(animation_steps[active_step].color);
            break;
          case animation_step::TWO: 
            setRightLeftColors(animation_steps[active_step].color, animation_steps[active_step].color2);
            break;
          case animation_step::THREE:
            setThreeColors(animation_steps[active_step].color,
                     animation_steps[active_step].color2,
                     animation_steps[active_step].color3);
            break;
          default:
            break;
        }
        active_step = animation_steps[active_step].next;
        break;
      default: 
        debug(F("other"));
        setSingleColor(strip.Color(255,0,128));
        // TO DO?
        break;
    }
  } else {
    setSingleColor(strip.Color(0,0,0)); // off
  }
  // debug("strip.show");
  strip.show();
}


/**
 * Set all pixels to the same color.
 */
void setSingleColor(uint32_t color) {
  strip.fill(color, 0, LED_COUNT);
}

/** 
 * Set Two colors, one for left half, one for right half.
 */
void setRightLeftColors(uint32_t left, uint32_t right) {
  for (int i = 0; i < LED_COUNT/2; i++) {
    strip.setPixelColor(pinTranslate(i), left);              // Front 
    strip.setPixelColor(pinTranslate(i+LED_COUNT/2), left);  // Back
   }
  for (int i = LED_COUNT/2; i < LED_COUNT; i++) {
    strip.setPixelColor(pinTranslate(i), right);             // Front
    strip.setPixelColor(pinTranslate(i+LED_COUNT/2), right); // Back
  }
}

/**
 * Set three colors (for each of the usual sectors of a Lighthous)
 * 
 *          0   
 *         /
 *       2 2 2         
 *     1       3     
 *    1         3
 *   1           3
 *    1         3
 *     1       3
 *       2 2 2
 */
void setThreeColors(uint32_t col1, uint32_t col2, uint32_t col3) {
  // Left Color
  strip.fill(col1, 9, 6); // Front
  strip.fill(col1, 9+LED_COUNT/2, 6); // Back
  // Middle Color
    // Front
  strip.fill(col2, 0, 2);
  strip.fill(col2, 7, 3);
  strip.setPixelColor(col2, 15);
    // Back
  strip.fill(col2, 0+LED_COUNT/2, 2);
  strip.fill(col2, 7+LED_COUNT/2, 3);
  strip.setPixelColor(col2, 15+LED_COUNT/2);
  
  // Right Color
  strip.fill(col3, 2, 6); // Front
  strip.fill(col3, 2+LED_COUNT/2, 6); // Back
}

/**
 * Set front and back color
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
 * Rotate two colors on each ring
 * 
 * First 8 are one color (left), Second 8 are other color (right),
 * then rotate them around the ring. 
 * 
 */
void rotateTwoColors(uint32_t left, uint32_t right)  {
  int rot = rotate % (LED_COUNT/2);

  for (int i = 0; i < LED_COUNT/2; i++) { // 0-15
    if ( ((i+rot)/(LED_COUNT/4))%2 == 0) {
      // 0-7
      strip.setPixelColor(pinTranslate(i), left);
      strip.setPixelColor(pinTranslate(i+LED_COUNT/2), left);
    } else {
      // 8-15
      strip.setPixelColor(pinTranslate(i), right);
      strip.setPixelColor(pinTranslate(i+LED_COUNT/2), right);
    }
  }
}

/**
 * Sweep
 * 
 * Interpolate linearly between two colors. 
 * Sweep back and forth. 
 */

void sweepTwoColors(uint32_t sweep1, uint32_t sweep2) {
  static boolean down  = false;

  int rot = rotate % 128;
  if (down) {
    rot = 127 - rot;
  } 

  uint32_t R = (((sweep1>>16)&0xFF) * rot + ((sweep2>>16)&0xFF) * (127-rot))/127;
  uint32_t G = (((sweep1>> 8)&0xFF) * rot + ((sweep2>> 8)&0xFF) * (127-rot))/127;
  uint32_t B = (((sweep1    )&0xFF) * rot + ((sweep2    )&0xFF) * (127-rot))/127;
  // debug(String("rot:") + rot + " R:" + R + " G:" + G + " B:" + B);
  
  strip.fill(strip.Color(R,G,B), 0, LED_COUNT);

  if (rot == 127) {
    down = true;
  } else if (rot == 0) {
    down = false;
  }
  // debug(String(rot) + " " + down);
}

/**
 * Flash all the Pixels once
 * 
 * Retrieves current state and restores it after the flash.
 * Takes 3x wait time, as it is: save, off, flash, off, restore.
 */
void colorFlash(uint32_t color, int wait) {
  // Save current state
  uint32_t cache[LED_COUNT];
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
 
