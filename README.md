# lumibaer
Program for controlling a enhanced version of a Flötotto "Lumibär"

# Pimping the Lumibär

The Lumibär is ca. 40 cm high, formed like a bear and was originally was designed and developed by FLÖTOTTO (AFAICT) with a 12V halogen light. Nowadays, a LED-version is sold with a remote, which allows to set the color. 

As we wanted to have a rather unique version, I set out to create a rather nerdy version of it and am sharing my build here. 

Features:

* Uses an Arduino Nano with ESP8266 for WiFi connectivity (aka. Pretzelboard or NanoESP)
* Two Adafruit 16 Neopixel Rings for blinkenlights
* A button was added to enable a hands-on on/off and color selection.
* Supports various different modes:
    * **Single color mode**: acts as a normal light, with a color of your choosing.
	* **Sweep mode**: Transition between two colors.
	* **Lighthouse mode**: Turn your Lumibär into your favourite lighthouse by entering the ligth characteristic.
	* Optionally use different front and back colors.
	
As our Lumibär is opaque white, we refrain from supporting movements or using single LED patterns. 

# Circuitry

The parts used:

| Count | Part                | Comment                                                    |
|-------|---------------------|------------------------------------------------------------|
| 1x    | NanoESP             | aka Pretzelboard, a Arduino Nano + ESP8266 on a single PCB |
| 2x    | 16 Neopixel Ring    | WS2812B, must fit through hole in bottom                   |
| 1x    | Button Switch       | color to your liking                                       |
| 1x    | 470Ω Resistor       | recommended by Adafruit in Neopixel tutorial               |
| 1x    | 1000 µF Capacitator | recommended by Adafruit in Neopixel tutorial               | 
| 1x    | Power Supply        | 100-250V in, 5V out, at least 1.5A                         |

The parts are connected like this:

![Fritzing circuit diagram](LumibaerSketch.png "Fritzing Curcuit Diagram for Lumibär")

