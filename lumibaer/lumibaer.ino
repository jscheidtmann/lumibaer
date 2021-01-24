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
 * Colors for wave mode (up and down): Color to start with
 */
uint32_t wave1 = 0;

/**
 * Colors for wave mode (up and down): Color coming in and going out
 */
uint32_t wave2 = 0;

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
  MODE_WAVE,         // Wave going up and down
  MODE_LIGHTHOUSE,   // Parse and show a lighthouse characteristic (Leuchtfeuer Kennung)
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
  unsigned long duration;
  enum step_mode { 
    SINGLE, // 0
    TWO,    // 1
    THREE   // 3
  } mode;
  uint32_t color;
  uint32_t color2;
  uint32_t color3;
  int next;
} animation_steps[10];

int active_step = 0;


const String & morse_string = (String) NULL;
int morse_pos = 0;



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
  p->duration = 1000;
  p->next = 1;
  p->color = strip.Color(255,255,255); // White
  p++;
  p->mode = animation_step::SINGLE;
  p->duration = 5500;
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
 *       "lighthouse?Oc(3) 5s RGW":
 *          Turns on lighthouse mode. Parses a lighthouse's characteristic into animation_steps, 
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
 *       "wave?ffddee,aabbcc":
 *          Start with color ffddee (HTML RGB code, leave out the "#". 
 *          then let other color aabbcc swell up and down like a wave
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
      // debug(F("brightness"));
      int brightness = strtol(request.substring(12).c_str(), NULL, 10);
      if (brightness < 0) brightness = DEFAULT_BRIGHTNESS;
      if (brightness > 255) brightness = 255;
      strip.setBrightness(brightness);
    } else if (request.startsWith(F(":rotwait?"))) {
      // debug(F("rotwait"));
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
    } else if (request.startsWith(F(":wave?"))) {
      lumibaer_mode = MODE_WAVE;
      lumibaer_state = true;
      rotate=0;
      wave1 = strtoul(request.substring(6, 12).c_str(), NULL, 16);
      wave2 = strtoul(request.substring(13, 19).c_str(), NULL, 16);
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
      case MODE_WAVE:
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
          active_step = 0;
          // switch on immediately:
          synchronizeStrip();
          rotate_wait = animation_steps[active_step].duration;
        } else if ((now - last_rotate) > rotate_wait) {
          last_rotate = now;
          active_step = animation_steps[active_step].next;
          synchronizeStrip();
          rotate_wait = animation_steps[active_step].duration;
          if (active_step < 0) 
            nextMorseCharacter();
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
 * Lighthouse modes.
 */
enum lh {
  FIXED,
  FLASH,
  LONG_FLASH, 
  OCCULTING,
  ISO,
  QUICK,
  VERYQUICK
} lh_m = lh::FIXED; 

int lh_groups = 1;

struct lh_color {
  char color_ch; 
  uint32_t color;
} lh_colors[] = { 
  { 'W', strip.Color(255,255,255) },
  { 'R', strip.Color(255,  0,  0) },
  { 'G', strip.Color(  0,255,  0) },
  { 'B', strip.Color(  0,  0,255) },
  { 'Y', strip.Color(  0,255,255) }
};

int lh_cycle = 0; // Cycle time in ms, e.g. 6500 for 6.5s.

String lh_colspec = (String) NULL;

/**
 * Oc(2)WRG.9s
 * 
 * BNF of a Lighthouse characteristic, see https://en.wikipedia.org/wiki/Light_characteristic
 * 
 * characteristic = mode group? color? cycle? .
 * mode           = "F" | "Iso" | "Fl" group? | "L.Fl" group? | "Oc" group? | "Q" group? | "VQ" group? .  
 *   // No group and cycle for "F". 
 *   // No cylce for "Q" and "VQ" when standalone, but if a group is specified, it becomes mandatory.
 * group          = "(" posnumber ")" .  
 * color          = colcode+ .
 * colcode        = "W" | "R" | "G" | "B" | "Y" . 
 * cycle          = float "s" .
 * float          = posnumber+ ( "." number+ )? .    // Note: we use one of the string to number functions, so you can ignore the rest of the spec.
 * posnumber      = notnull number* .
 * notnull        = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" . 
 * number         = "0" | notnull .
 * 
 * Note: US light characteristic listing at http://uslhs.org uses a different way of specifying the characteristic.
 * 
 * Durations:
 *    L.Fl = 3s on per L.Fl
 *    Oc + F = 1s on per Flash
 *    Q = 1 "on" per Second (500 ms)
 *    VQ = 2x "on" per Second (250 ms)
 * 
 * Some Examples: 
 *              Amrum: Fl 6.5s 
 *          Helgoland: Fl 5s
 *       Süderoogsand: Iso WR 6s
 *    Westerheversand: Oc(3) WRG 15s
 *     Folkstone (UK): Fl(2) 10s
 *       Danger North: Q
 *       Danger South: Q(6)+L.Fl 15s  <<== TODO: Cannot be done with this parser.
 *        Danger East: Q(9)15s
 *        Danger West: Q(3)15s
 *     (Danger signs can also be VQ instead of Q)
 */
void parseLighthouseSpec(const String & spec) {
  // Remove old state
  lh_colspec = "";
  
  int pos = 0;

  // Only use a single line: 
  spec = spec.substring(0, spec.indexOf("\n"));
  debug(F("parseLightHouseSpec"));
  debug(spec);
  if (!parse_lh_mode_group(spec, pos)) {
    debug(F("Wrong Mode"));
    return;
  }
  debug(String(pos));
  debug(F("mode_group ok"));
  if (!parse_lh_colors(spec,pos)) {
    debug(F("Wrong colspec"));
    return;
  }
  debug(String(pos));
  debug(F("colors ok"));
  if (!parse_lh_cycle(spec,pos)) {
    if (lh_m == lh::FIXED || lh_m == lh::QUICK || lh_m == lh::VERYQUICK) {
      lh_groups=1;
      if (lh_m == lh::QUICK) {
        lh_cycle = 500*2;        
      } else if (lh_m == lh::VERYQUICK) {
        lh_cycle = 250*2;
      }
    } else {
      debug(F("Wrong cycle spec"));
      return;
    }
  }
  debug(String(pos));
  debug(F("cycle ok"));
  
  // / *
  debug(F("*** Parse Result: mode, colspec, groups, cycle (ms)"));
  debug(String(lh_m));
  debug(lh_colspec);
  debug(String(lh_groups));
  debug(String(lh_cycle));
  // */ 

  compile_lh();

  dump_animation();
}

void dump_animation() {
  debug(F("************* Dump animation **********"));
  for (int i = 0; i < sizeof(animation_steps)/sizeof(animation_step); i++) {
    animation_step s = animation_steps[i];
    debug(F("*** #, mode, duration, next, col, col2, col3"));
    debug(String(i));
    debug(String(s.mode));
    debug(String(s.duration));
    debug(String(s.next));
    debug(String(s.color, 16));
    debug(String(s.color2, 16));
    debug(String(s.color3, 16));
  }
}

boolean parse_lh_mode_group(const String & spec, int & pos) {
  const String next = spec.substring(pos);
  if (next.startsWith("L.Fl")) {
    lh_m = lh::LONG_FLASH;
    pos += 4;
  } else if (next.startsWith("Fl")) {
    lh_m = lh::FLASH;
    pos += 2;
  } else if (next.startsWith("F")) {
    lh_m = lh::FIXED;
    pos += 1;
  } else if (next.startsWith("Oc")) {
    lh_m = lh::OCCULTING;
    pos += 2;
  } else if (next.startsWith("Iso")) {
    lh_m = lh::ISO;
    pos += 1;
  } else if (next.startsWith("Q")) {
    lh_m = lh::QUICK;
    pos += 1;
  } else if (next.startsWith("VQ")) {
    lh_m = lh::VERYQUICK;
    pos += 2;
  } else {
    return false;
  }

  if (spec[pos] == '(' && (lh_m == lh::FIXED || lh_m == lh::ISO)) {
    return false;
  } else if (spec[pos] == '(' && lh_m != lh::FIXED && lh_m != lh::ISO) {
    return parse_lh_group(spec, pos);
  } else {
    lh_groups = 1;
    // pos increase already happened in switch!
    return true;
  }
}

boolean parse_lh_group(const String &spec, int & pos) {

  if (spec[pos] == '(') {
    pos++;
  } else {
    return false;
  }
  lh_groups = strtol(spec.substring(pos).c_str(), NULL, 10);
  if (lh_groups <= 1) {
    return false;
  } else { 
    pos = spec.indexOf(")", pos); 
    if (-1 != pos) {
      pos++;
      return true;
    } else {
      return false;
    }
  }
}

boolean parse_lh_colors(const String &spec, int & pos) {
  debug(F("parse_lh_colors"));
  debug(spec.substring(pos));
  
  // Avoid parsing, if we already found something.
  if (lh_colspec != "") {
    debug(F("early ret true"));
    return true;
  }

  int start_pos = pos; 
  boolean is_color = false; 
  debug(F("loop"));
  do {
    is_color = false;
    debug(String(spec[pos]));
    for (int i = 0; i < sizeof(lh_colors)/sizeof(struct lh_color); i++) {
      if (spec[pos] == lh_colors[i].color_ch) {
        is_color = true;
        pos++;
        break;
      }
    }
  } while (is_color);

  if (pos > start_pos) {
    lh_colspec = spec.substring(start_pos, pos);
    debug(F("colspec"));
    debug(lh_colspec);
  } else {
    lh_colspec = String("W");
    debug(F("colspec = W"));
  }
  return true;
}

boolean parse_lh_cycle(const String &spec, int & pos) {
  debug(F("parse_lh_cycle"));
  debug(spec);
  debug(String(pos));
  
  float duration = spec.substring(pos).toFloat();
  if (duration > 0.f) {
    lh_cycle = (int) (duration * 1000);
    pos = spec.indexOf("s", pos)+1;
    return -1 != pos && lh_cycle > 0;
  } else {
    return true;
  }
}

void compile_lh() {
  int max = sizeof(animation_steps)/sizeof(animation_step);
  if (2*lh_groups > max) lh_groups = max/2;
  
  // Initialize animation_steps to all off 
  for (int i = 0; i < max; i++) {
    animation_steps[i].mode = animation_step::step_mode::SINGLE;
    animation_steps[i].color = strip.Color(0,0,0); // Black
    animation_steps[i].duration = 250;
    animation_steps[i].next = i+1;
  }
  animation_steps[max-1].next = 0;

  // Now create the animation_steps.
  int total = lh_cycle;
  int inverted = 0; // 0 if not inverted, 1 if inverted
  for (int i = 0; i < lh_groups; i++) {
    switch (lh_m) {
      case lh::FIXED: 
         animation_steps[0].duration = 10000;
         animation_steps[0].next = 0;
         lh_step_set_colors(animation_steps[0]);
         break;
      case lh::FLASH:
         animation_steps[2*i].duration = 1000;
         animation_steps[2*i].next = 2*i+1;
         lh_step_set_colors(animation_steps[2*i]);
         total -= 1250;
         break;
      case lh::LONG_FLASH:
         animation_steps[2*i].duration = 3000;
         animation_steps[2*i].next = 2*i+1;
         lh_step_set_colors(animation_steps[2*i]);
         total -= 3250;
         break;
      case lh::QUICK:
         animation_steps[2*i].duration = 500;
         animation_steps[2*i].next = 2*i+1;
         lh_step_set_colors(animation_steps[2*i]);
         total -= 750;
         break;
      case lh::VERYQUICK:
         animation_steps[2*i].duration = 250;
         animation_steps[2*i].next = 2*i+1;
         lh_step_set_colors(animation_steps[2*i]);
         total -= 500;
         break;
      case lh::OCCULTING:
         animation_steps[2*i+1].duration = 1000;
         animation_steps[2*i+1].next = 2*i+2;
         lh_step_set_colors(animation_steps[2*i+1]);
         total -= 1250;
         break;
      default: 
         break;
    } // Switch
  } // for
  if (total > 0) {
    animation_steps[2*lh_groups-1].duration = total;
    animation_steps[2*lh_groups-1].next = 0;
  }
}

void lh_step_set_colors(struct animation_step & step) {
  int len = lh_colspec.length();
  if (len <= 3) 
    step.mode = (animation_step::step_mode) (len-1);
  else 
    step.mode = animation_step::step_mode::THREE;

  if (len >= 1) step.color = lh_get_color(lh_colspec[0]);
  if (len >= 2) step.color2 = lh_get_color(lh_colspec[1]);
  if (len >= 3) step.color3 = lh_get_color(lh_colspec[2]);     
}

uint32_t lh_get_color(char ch) {
    for (int i = 0; i < sizeof(lh_colors)/sizeof(lh_color); i++) {
      if (lh_colors[i].color_ch == ch) {
        return lh_colors[i].color;
      }
    }
    // Else return black.
    return strip.Color(0,0,0);
}

// Morse -------------------------------------------------------------------

/**
 * SOS = ... --- ...
 * 
 */
void parseMorseSpec(const String & spec) {
  String copy = String(spec);
  copy.toLowerCase();
  // Add Calling all stations to front and <OVER> to end
  morse_string = String("cq cq cq ") + copy + String(" k ");
  morse_pos = 0; 
}


void nextMorseCharacter() {
  if (morse_pos > morse_string.length()) {
    // TODO: Send OVER and OUT symbols (<AR>) 
    morse_out();
    morse_pos = 0;
  }
  char ch = morse_string[morse_pos++];
  morse_send(ch);
}

void morse_out() {
  // TODO: <AR> Send A and R, but with no spacing.
}

void morse_send(char ch) {
  // TODO: Translate to morse code and store in animation_steps.
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
        break;
      case MODE_WAVE:
        waveColors(wave1, wave2);
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
  for (int i = 0; i < LED_COUNT/4; i++) {
    strip.setPixelColor(pinTranslate(i), left);              // Front 
    strip.setPixelColor(pinTranslate(i+LED_COUNT/2), left);  // Back
   }
  for (int i = LED_COUNT/4; i < LED_COUNT/2; i++) {
    strip.setPixelColor(pinTranslate(i), right);             // Front
    strip.setPixelColor(pinTranslate(i+LED_COUNT/2), right); // Back
  }
}

/**
 * Set three colors (for each of the usual sectors of a Lighthouse)
 * 
 *          zero   
 *         |   
 *       2 2 2---- one         
 *     1       3     
 *    1         3
 *   1           3
 *    1         3
 *     1       3
 *       2 2 2
 * 
 * TODO: This routine depends on using NeoPixel Rings of size 16.
 */
void setThreeColors(uint32_t col1, uint32_t col2, uint32_t col3) {
  // Left Color
  for (int i = 10; i < 15; i++) {
    strip.setPixelColor(pinTranslate(i), col1); // Front
    strip.setPixelColor(pinTranslate(i+LED_COUNT/2), col1); // Back
  }
  // Middle Color
    // Front
  strip.setPixelColor(pinTranslate(0), col2);
  strip.setPixelColor(pinTranslate(1), col2);
  for (int i = 7; i <7+3; i++) strip.setPixelColor(pinTranslate(i), col2);
  strip.setPixelColor(pinTranslate(15), col2);
    // Back
  strip.setPixelColor(pinTranslate(0+LED_COUNT/2), col2);
  strip.setPixelColor(pinTranslate(1+LED_COUNT/2), col2);
  for (int i = 7; i <7+3; i++) strip.setPixelColor(pinTranslate(i+LED_COUNT/2), col2);
  strip.setPixelColor(pinTranslate(15+LED_COUNT/2), col2);
  // Right Color
  for (int i = 2; i < 2+5; i++) {
    strip.setPixelColor(pinTranslate(i), col3); // Front 
    strip.setPixelColor(pinTranslate(i+LED_COUNT/2), col3); // Back 
  }
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
 * Wave colors
 *  
 *           Position 0
 *          /       |--- t ---- X indicating line is on ---->
 *         |   rot:           1         2         3            ... 
 *         |        01234567890123456789012345678901234567     ...      
 *       0 0 0             X             X             X
 *     0       0          XXX           XXX           XXX
 *    0         0        XXXXX         XXXXX         XXXXX 
 *   0           0      XXXXXXX       XXXXXXX       XXXXXX 
 *    0         0      XXXXXXXXX     XXXXXXXXX     XXXXXXX 
 *     0       0      XXXXXXXXXXX   XXXXXXXXXXX   XXXXXXXX 
 *       0 0 0       XXXXXXXXXXXXX XXXXXXXXXXXXX XXXXXXXXX      ...
 *          \ 
 *           LED_COUNT/4
 *    
 * You get the idea...
 *
 * 0 = only first color
 * 1 = 1 LED
 * 7
 */
void waveColors(uint32_t col1, uint32_t col2) {
  int rot = rotate % (LED_COUNT/2 + 2); // Is always >0 (rotate being unsigned)

  // debug(String(rot));
  if (rot == 0) {
    strip.fill(col1, 0, LED_COUNT);
  } else if (rot <= LED_COUNT/4) {
    // debug(F("First wave"));
    strip.fill(col1, 0, LED_COUNT);
    
    // Second color. 
    // Starting from a pixel (LED_COUNT/4), in each step add an additional pixel left and right.
    // Is stopped before the color can overflow to the back, by the if condition above.
    for (int i = LED_COUNT/4 - (rot-1); i <= LED_COUNT/4 + (rot-1); i++) {
      strip.setPixelColor(pinTranslate(i), col2); // Front
      strip.setPixelColor(pinTranslate(i+LED_COUNT/2), col2); // Back
    }
  } else if (rot == LED_COUNT/4+1) {
    // debug(F("fill col2"));
    strip.fill(col2, 0, LED_COUNT);
  } else {
    // debug(F("Second wave"));
    strip.fill(col1, 0, LED_COUNT);
    
    // Same as first wave, but count down.
    for (int i = LED_COUNT/4 - (LED_COUNT/2+1-rot); i <= LED_COUNT/4 + (LED_COUNT/2+1-rot); i++) {
      strip.setPixelColor(pinTranslate(i), col2); // Front
      strip.setPixelColor(pinTranslate(i+LED_COUNT/2), col2); // Back      
    }
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

  // Swell up
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
 
