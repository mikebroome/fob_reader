/*
 * fob_reader - a simple sketch to read RFID fobs using HID reader
 *   hardware is Arduino Uno with (optional) Adafruit RGB LCD shield (I2C connected)
 *
 *   Note: Arduino Uno has ATmega328 chip - http://arduino.cc/en/Main/ArduinoBoardUno
 *
 * based loosely on code from ...
 *   Open Source RFID Access Controller - v3 Standard Beta hardware
 *
 *   9/3/2013 Version 1.5
 *   Last build test with Arduino v1.04
 *   Arclight - arclight@23.org
 *   Danozano - danozano@gmail.com
 *
 *   For latest downloads, including Eagle CAD files for the hardware, check out
 *   http://code.google.com/p/open-access-control/downloads/list
 *
 * This program interfaces the Arduino to RFID reader using the Wiegand-26 Communications
 * Protocol. Ideally (i.e. recommended practice) is that the RFID reader inputs be
 * opto-isolated in case a malicious user shorts out the input device.
 *
 */

#include "user.h"         // User preferences file; use this to select hardware options, etc.
#include <WIEGAND26.h>    // Wiegand 26 reader format libary

#ifdef LCDBOARD
#include <Wire.h>         // Needed for I2C bus
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#endif

#ifdef MCU328
#include <PCATTACH.h>     // Pcint.h implementation, allows for >2 software interupts.
#endif


//
// HID RFID reader pin definitions and notes
//
// NOTE: lines to the HID reader seem (experimentally) to be active-low
//
// NOTE: keeping HOLD line low inhibits reader from working
//   it will beep once when a key is swiped, but it won't read again until HOLD line is raised high
//   (or floating).  the key id that was read will be buffered until the HOLD line is released then
//   will come in over DATA0 and DATA1 lines
//
// NOTE: setting BEEP CNTRL (yellow) wire high turns the beeper on and keeps
//   it on until it is set to low again
//
// NOTE: if using the Serial functionality to print and communicate via
//   over Serial Monitor, can't use digitial pins 0, 1 for anything else
//   since they mirror the serial line data that's also being sent
//   over the USB interface
// see http://arduino.cc/en/Reference/Serial
//
// from http://arduino.cc/en/Reference/AttachInterrupt
// Most Arduino boards have two external interrupts:
//   0 (on digital pin 2)
//   1 (on digital pin 3)
//

int DATA0_INT = 0;      // DATA0 is green wire, connected to pin 2
int DATA1_INT = 1;      // DATA1 is white wire, connected to pin 3

int DATA0_PIN     = 2;  // green wire
int DATA1_PIN     = 3;  // white wire
int GREEN_LED_PIN = 4;  // orange wire
int RED_LED_PIN   = 5;  // brown wire
int BEEP_CTRL_PIN = 6;  // yellow wire
int HOLD_PIN      = 7;  // blue wire
int CARD_PRESENT  = 8;  // violet wire

// Vcc = red wire
// GND = black wire
// shield = black (braided) wire
int VCC_PIN       = 12; // red wire - set to -1 if directly connected to +5V rather than a digital output


#ifdef HWV3STD                          // Use these pinouts for the v3 Standard hardware

#define READER1GRN     10
#define READER1BUZ     11
#define READER2GRN     12
#define READER2BUZ     13

#define R1ZERO          2	// DATA0 (green)
#define R1ONE           3	// DATA1 (white)
#define R2ZERO          4
#define R2ONE           5

uint8_t reader1Pins[] = { R1ZERO, R1ONE };  // Reader 1 pin definition
uint8_t reader2Pins[] = { R2ZERO, R2ONE };  // Reader 2 pin definition

#else

uint8_t reader1Pins[] = { DATA0_PIN, DATA1_PIN };  // Reader 1 pin definition

#endif


//
// Global values for the Wiegand RFID readers
//
volatile long reader1 = 0;       // Reader1 buffer
volatile int  reader1Count = 0;  // Reader1 received bits counter
volatile long reader2 = 0;
volatile int  reader2Count = 0;


//
// Create an instance of the various C++ libraries we are using.
//

WIEGAND26 wiegand26;  // Wiegand26 (RFID reader serial protocol) library


#ifdef MCU328
PCATTACH pcattach;    // Software interrupt library
#endif

#ifdef LCDBOARD
// The LCD shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
#endif


#ifdef LCDBOARD
void lcdStatus(uint8_t door, bool status)
{
  delay(10);
  if (door == 1) {
    lcd.setCursor(0,0);
  } else {
    lcd.setCursor(0,1);
  }
  lcd.print("Door ");  
  lcd.print(door, DEC);
  if (status==false) {
    lcd.print(" unlocked.");
  } else {
    lcd.print("   locked.");    
  }
}
#endif


long decodeCard(long input) {
  if (CARDFORMAT==0) {
    return(input);
  }
  
  if (CARDFORMAT==1) {
    bool parityHigh;
    bool parityLow;
    parityLow=bitRead(input,0);
    parityHigh=bitRead(input,26);
    bitWrite(input,25,0);        // Set highest (parity bit) to zero
    input=(input>>1);            // Shift out lowest bit (parity bit)

/*
    bool parityTemp0=0;
    for (unsigned int i=0; i<13; i++) {
      parityTemp0+=bitRead(input,i);
    }
    
    bool parityTemp1=0;
    for (unsigned int i=14; i<24; i++) {
      parityTemp1+=bitRead(input,i);
    }
 
    if ( (parityTemp0 != parityLow) && (parityTemp1 != parityHigh) ) {
      return(input);
    } else {
      Serial.print("ParityTemp0: "); Serial.print(parityTemp0); Serial.print(' ');Serial.println(parityLow); 
      Serial.print("ParityTemp1: "); Serial.print(parityTemp1); Serial.print(' ');Serial.println(parityHigh); 
      return(-1);
    }
 */
    return(input);
  }
}


//
// Wrapper functions for interrupt attachment
//
void callReader1Zero() { wiegand26.reader1Zero(); }
void callReader1One()  { wiegand26.reader1One();  }
void callReader2Zero() { wiegand26.reader2Zero(); }
void callReader2One()  { wiegand26.reader2One();  }
void callReader3Zero() { wiegand26.reader3Zero(); }
void callReader3One()  { wiegand26.reader3One();  }


// setup() - Runs once at Arduino boot-up
void setup() {

#ifdef LCDBOARD
  Wire.begin();   // start Wire library as I2C-Bus Master
#endif

  // Initialize the Arduino built-in pins
  pinMode(DATA0_PIN,INPUT);  // reader 1 DATA0 input / interrupt
  pinMode(DATA1_PIN,INPUT);  // reader 1 DATA1 input / interrupt

  if (VCC_PIN != -1) {
    // power (red wire) for RFID reader
    pinMode(VCC_PIN,OUTPUT);
    digitalWrite(VCC_PIN, HIGH);
  }
   
  //
  // Attach pin change interrupt service routines from the Wiegand RFID readers
  //
#ifdef MCU328
  pcattach.PCattachInterrupt(reader1Pins[0], callReader1Zero, CHANGE); 
  pcattach.PCattachInterrupt(reader1Pins[1], callReader1One,  CHANGE);  
  //pcattach.PCattachInterrupt(reader2Pins[0], callReader2Zero, CHANGE);
  //pcattach.PCattachInterrupt(reader2Pins[1], callReader2One,  CHANGE);
#endif

  //start serial port and print information for debugging
  Serial.begin(UBAUDRATE);
  Serial.print("fob reader v");
  Serial.println(VERSION);
  Serial.println();

#ifdef LCDBOARD
  lcd.begin(16,2);
  lcd.setCursor(0,0);
  lcd.print("fob reader v");
  lcd.print(VERSION);
  delay(3000);
  lcd.clear();
#endif

  //Clear and initialize readers
  wiegand26.initReaderOne();  // Set up Reader 1 and clear buffers
  //wiegand26.initReaderTwo();  // Set up Reader 2 and clear buffers 
 
}


// loop() - Main branch, runs over and over again
void loop() {                         
  long reader1_short;
  
#ifdef LCDBOARD
  //lcdStatus(1,door1Locked);
  //lcdStatus(2,door2Locked);
#endif

  // Notes: RFID polling is interrupt driven, just test for the reader1Count value to climb to the bit length of the key
  // change reader1Count & reader1 et. al. to arrays for loop handling of multiple reader output events
  // later change them for interrupt handling as well!
  // currently hardcoded for a single reader unit

  // NOTE: for some reason, the first read is always wrong
  //       looks like it's off by one bit (right shifted by 1, upper don't care bits can also be wrong)
  //
  //       moving wiegand26.initReaderOne() and wiegand26.initReaderTwo() calls to the
  //       end of the steup() block fixed the problem ... but only if there's an LCD shield connected (?!?)
  //
  //       "feels" like some sort of timing problem

  // checks a reader with a 26-bit keycard input
  if (reader1Count >= 26) {       //  When tag presented to reader1 (No keypad on this reader)
    reader1=decodeCard(reader1);  // Format the card data (format can be defined in user.h)

    // short ID that's printed on the fob is in bits [16:1]
    reader1_short = (reader1 >> 1) & 0x0000FFFF;

    Serial.print("short id: ");
    Serial.print(reader1_short, DEC);
    Serial.println();
    Serial.print("long  id: ");
    Serial.print(reader1, DEC);
    Serial.print(", 0x");
    Serial.print(reader1, HEX);
    Serial.print(", ");
    Serial.print(reader1, BIN);
    Serial.println('b');

#ifdef LCDBOARD
    lcd.clear();
    lcd.print(reader1_short, DEC);
#endif

    wiegand26.initReaderOne();  // Reset for next tag scan
  }
}

