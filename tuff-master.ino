#include <ArduinoJson.h>
#include <EEPROM.h>
#include <SPI.h>
#include "driverlib/uart.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "inc/hw_flash.h"
#include "driverlib/sysctl.h"

#define VERSION "2.1"

#define RESET_ACTIVE 1
#define RESET_INACTIVE 0

// Pins.
#define RX_LED 0
#define TX_LED 1
#define BOOT_LED 2
#define LED_ON_TIME 50
unsigned long led_off_time[3] = { 0, 0, 0 };
const unsigned long led_pin[3] = { 30, 39, 40 };
const unsigned long tuff_reset[2] = { 27, 33 };
const unsigned long tuff_clock[2] = { 11, 23 };

bool debug = false;
bool quiet = false;

// SPI to the TUFFs has CPOL=1 (clock idle high) and CPHA=1.

// pins:
// SCK0 = PA_2
// MOSI0 = PA_5
// SCK1 = PF_2
// MOSI1 = PF_1
// RX0 = 
int led_toggle =0;

// TUFF addresses go
// 01, 02, 04, 08, 10, 20
// 41, 42, 44, 48, 50, 60

// My information.
// Integer 0: iRFCM number
uint32_t __attribute__((section(".noinit"))) irfcm;

// Integer 1: LoTUFF0 ch0 phi   (1<<0)   
// Integer 2: LoTUFF0 ch1 phi   (1<<1)
// Integer 3: LoTUFF0 ch2 phi   (1<<2)
// Integer 4: LoTUFF0 ch3 phi   (1<<3)
// Integer 5: LoTUFF0 ch4 phi   (1<<4)
// Integer 6: LoTUFF0 ch5 phi   (1<<5)
// Integer 7: HiTUFF0 ch0 phi   (1<<7 | 1<<0)
// Integer 8: HiTUFF0 ch1 phi   (1<<7 | 1<<1)   
// Integer 9: HiTUFF0 ch2 phi   (1<<7 | 1<<2)
// Integer10: HiTUFF0 ch3 phi   (1<<7 | 1<<3)
// Integer11: HiTUFF0 ch4 phi   (1<<7 | 1<<4)
// Integer12: HiTUFF0 ch5 phi   (1<<7 | 1<<5)
uint32_t __attribute__((section(".noinit"))) phi_array[24];
uint32_t __attribute__((section(".noinit"))) tuff_default_array[5];
char __attribute__((section(".noinit"))) cmd_buffer[512];
unsigned char cmd_buffer_ptr = 0;

void led_blink_on(unsigned int pin) {
  if (!digitalRead(led_pin[pin])) {
    digitalWrite(led_pin[pin],1);
    led_off_time[pin] = millis() + LED_ON_TIME;
  }
}
void led_blink_off_check() {
   unsigned int i;
   for (i=0;i<3;i=i+1) {
     if (digitalRead(led_pin[i]))
       if ((long) (millis() - led_off_time[i]) > 0)
         digitalWrite(led_pin[i],0);
   }
}

// NOTE: This is BACKWARDS from the original schematic/layout,
// however, it matches the LABELING on the board, which
// swapped 0/2 (literally, "TUFF 0" is over the connector
// labelled 'TUFF23_RESET', etc.
SPIClass tuff0(3);
SPIClass tuff2(0);

void tuffCommand(unsigned int tuff, unsigned int command) {

/* blah blah blah debugging code that might be useful later so I'm just gonna leave it here. 
   -- Cosmin
  char buf[64]; 
  sprintf(buf, "{\"cmd\": [%u, %x]}", tuff, command); 
  Serial.println(buf); 
*/


  if (tuff) {
    tuff2.transfer((command & 0xFF00)>>8);
    tuff2.transfer(command & 0xFF);
  } else {
    tuff0.transfer((command & 0xFF00)>>8);
    tuff0.transfer(command & 0xFF);
  }
}

void resetAll() {
  digitalWrite(tuff_reset[0], RESET_ACTIVE);
  digitalWrite(tuff_reset[1], RESET_ACTIVE);
  digitalWrite(tuff_reset[0], RESET_INACTIVE);
  digitalWrite(tuff_reset[1], RESET_INACTIVE);
  // Sleep to make sure everyone's awake.
  delay(50);
  // OK, we're awake. Now synchronize them (send 0xD00D).
  tuffCommand(0, 0xD00D); 
  tuffCommand(1, 0xD00D);  
}

// Reserved bits.
#define BOOTCFG_MASK 0x7FFF00EC
// Value to be written.
// NW = 0, PORT = B (001), PIN = 5 (101), POL = 0 (LOW), EN = 0,
// KEY = 1, DBG1=1,DB0=0.
// so byte 1 is 001 101 0 0 = 0x34
// 0x7FFF34FE
#define TUFF_MASTER_BOOTCFG 0x7FFF34FE

void setup()
{
  unsigned char nb;
  unsigned int tmp;
  
  pinMode(led_pin[0], OUTPUT);
  pinMode(led_pin[1], OUTPUT);
  pinMode(led_pin[2], OUTPUT);

  pinMode(tuff_clock[0], OUTPUT);
  pinMode(tuff_clock[1], OUTPUT);
  digitalWrite(tuff_clock[0], 1);
  digitalWrite(tuff_clock[1], 1);

  Serial.begin(115200);
  Serial4.begin(115200);

  
  
  ROM_EEPROMInit();
  ROM_EEPROMRead(&irfcm, 0, sizeof(irfcm));
  ROM_EEPROMRead(phi_array, sizeof(irfcm), sizeof(phi_array));
  ROM_EEPROMRead(tuff_default_array, sizeof(irfcm)+sizeof(phi_array), sizeof(tuff_default_array));
  

  // Set up SPI outputs.
  tuff0.begin();
  tuff2.begin();
  tuff0.setDataMode(SPI_MODE3);
  tuff0.setClockDivider(16);
  tuff2.setDataMode(SPI_MODE3);
  tuff2.setClockDivider(16);
  // Send 0xFFFF just in case reset's not working (debugger!).
  tuffCommand(0, 0xFFFF);
  tuffCommand(1, 0xFFFF);
  tuffCommand(0, 0xFFFF);
  tuffCommand(1, 0xFFFF);
  // Set up RESET outputs, and toggle RESET.
  digitalWrite(tuff_reset[0], RESET_INACTIVE);
  digitalWrite(tuff_reset[1], RESET_INACTIVE);
  pinMode(tuff_reset[0], OUTPUT);
  pinMode(tuff_reset[1], OUTPUT);
  resetAll();
  // Clear out our crap.
  nb = Serial.available();
  while (nb) {
    Serial.read();
    nb--;
  }
  nb = Serial4.available();
  while (nb) {
    Serial4.read();
    nb--;
  }
  // Sleep again to make sure the other TUFFs are ready.
  delay(50);
  // Defaults.
  // Are *any other* bits set in the TUFF default array? If so, we were 'blank'.
  if (tuff_default_array[4] & 0xFFFFFFF0) {
    tuff_default_array[4] = 0;
    tuff_default_array[0] = 0x3F89;
    tuff_default_array[1] = 0x7F89;
    tuff_default_array[2] = 0x3F89;
    tuff_default_array[3] = 0x7F89;
    ROM_EEPROMProgram(tuff_default_array, sizeof(irfcm)+sizeof(phi_array), sizeof(tuff_default_array));
  }
  for (unsigned int i=0;i<4;i++) {
    if (!(tuff_default_array[4] & (1<<i))) {
      unsigned int tuffNum;
      if (i == 0 || i == 1) tuffNum = 0;
      else tuffNum = 1;
      // This default is valid. Do it.
      tuffCommand(tuffNum, tuff_default_array[i]);
      delay(10);  //Added by Cosmin. 10 might be too long but who cares. I could also interleave them but if some of the defaults are disabled that won't work. 
    }
  }  
  // Check the BOOTCFG register.
  tmp = HWREG(FLASH_BOOTCFG);
  if (tmp & FLASH_BOOTCFG_NW) {
    Serial.println("{\"log\":\"updating BOOTCFG\"}");
    tmp = (tmp & BOOTCFG_MASK) | (TUFF_MASTER_BOOTCFG & ~BOOTCFG_MASK);
    HWREG(FLASH_FMD) = tmp;
    HWREG(FLASH_FMA) = 0x75100000;
    HWREG(FLASH_FMC) = FLASH_FMC_WRKEY | FLASH_FMC_COMT;
    while (HWREG(FLASH_FMC) & FLASH_FMC_COMT);
    Serial.println("{\"log\":\"BOOTCFG updated\"}");
  }  
  // Output a boot log.
  Serial.print("{\"log\":\"boot irfcm ");
  if (irfcm != 0xFFFFFFFF) {
    Serial.print(irfcm);
    Serial.println(" v" VERSION "\"}");
  } else {
    Serial.println("unassigned v" VERSION "\"}");
  }
  // I dunno why, but the first analogRead is crap.
  analogRead(TEMPSENSOR);  
  led_blink_on(BOOT_LED);
}

void loop()
{
  // Forward Serial2 -> Serial1.
  unsigned char nb;
  nb = Serial4.available();
  while (nb) {
    unsigned char c;
    c = Serial4.read();
    Serial.write(c);
    nb--;
  }
  nb = Serial.available();
  while (nb) {
    led_blink_on(RX_LED);
    char c = Serial.read();
    Serial4.write(c);
    // Check for newline.
    if (c == '\n') {
      // Terminate the command buffer at that point.
      cmd_buffer[cmd_buffer_ptr] = 0;
      // Parse the command.
      parseJsonCommand();
      // Reset command buffer pointer.
      cmd_buffer_ptr = 0;
    } else {
      // Add data to buffer.
      cmd_buffer[cmd_buffer_ptr] = c;
      // If we're not at the end, increment pointer.
      if (cmd_buffer_ptr < 127) cmd_buffer_ptr++;
    }
    // Decrement number of bytes to read.
    nb--;
  }
  // check to see if we need to turn off any LEDs
  led_blink_off_check();
}

// Commands
// -- {"ping":[0,1,2,3]}  - send ping request to iRFCM 0, 1, 2, 3.
//    expect response
//    {"pong":0}
//    {"pong":1}
//    {"pong":2}
//    {"pong":3}
//    The order tells you the order of the chain.
//    
//    NOTE: 'Set' functions (mostly) involve changing things that will survive power-off.
//          Every set entry (except for irfcm) gets its own "ack", if you send multiple.
//          Don't send lots of multiples.
// -- {"set":{"irfcm":0,"save":1}}
//    This sets iRFCM #0 to this master. Be careful with this, as 
//    if the chain is set up, it will set *all* of them to iRFCM #0!
//    To set only one, disconnect the chain. If 'save'=0, then this change
//    does not survive power-off.
//
// -- {"test":[0,0]}
//    tests iRFCM 0's stack 0.
//
// -- {"set":{"irfcm":0,"phi":[0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,2,2,3,3,3,3,3,3]}}
//    Sets iRFCM #0's TUFF channels to phi #0, 1, 2, and 3 respectively.
//    Phis go from 0->15.
//    Expect response:
//    {"ack":0}
// -- {"cap":[0,1,2,31]}
//    Sets the variable capacitor on notch 2 on channel 1 in iRFCM 0 to 31.
//    First argument is iRFCM.
//    Second is channel (0-23).
//    Third is notch number (0,1,2).
//    Fourth is cap state (0-31).
// 
// -- {"raw":[0,0,123456]}
//    Sends a raw TUFF command ("123456" here) on iRFCM 0's TUFF 0/1 stack.
//    A TUFF2/3 stack command would be [0,1,123456]
//    The 'raw' commands are documented elsewhere for the TUFF.
//    Expect response:
//    {"ack":0}
//
// -- {"on":[0,0,7]}
//    Turns on a notch for a specific channel in an iRFCM.
//    First number is iRFCM (so here, 0).
//    Second number is channel in the iRFCM (from 0-23).
//    Third number is a bitmask of the notches to turn on.
//    So here, 7 = all notches (111).
// 
// -- {"off":[0,0,7]}
//    See above, but turns off notches.
//
// -- {"r0":[0,15]}
//    Sets notch 0 on a phi-sector basis.
//    The first number is the start phi, so here start=0.
//    The second number is the end phi (inclusive) so here stop=15, so this would turn on notch0 for all phi sectors.
// -- {"r1":[0,15]}
//    Same as r0 for notch 1.
// -- {"r2":[0,15]}
//    Same as r0 for notch 2.
//
//
// Channels 0-11 map to TUFF#0.
// Channels 12-23 map to TUFF#1.
unsigned int tuff_ch_to_tuff(unsigned int channel) {
  if (channel < 12) return 0;
  return 1;
}
// Map channels 0-5   to 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000. (remember address is top 8 bits)
// Map channels 6-11  to 0x4100, 0x4200, 0x4400, 0x4800, 0x5000, 0x6000.
// Map channels 12-17 to 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000.
// Map chnanels 18-23 to 0x4100, 0x4200, 0x4400, 0x4800, 0x5000, 0x6000.

const unsigned int tuff_address_table[24] = {
  0x0100,
  0x0200,
  0x0400,
  0x0800,
  0x1000,
  0x2000,
  0x4100,
  0x4200,
  0x4400,
  0x4800,
  0x5000,
  0x6000,
  0x0100,
  0x0200,
  0x0400,
  0x0800,
  0x1000,
  0x2000,
  0x4100,
  0x4200,
  0x4400,
  0x4800,
  0x5000,
  0x6000
};

unsigned int tuff_ch_to_address(unsigned int channel) {
  return tuff_address_table[channel];
}

void build_mask(unsigned int start, unsigned int stop, unsigned short *on_mask, unsigned short *off_mask) {
  // Figure out which channels we turn on, and which we turn off.
  for (unsigned int i=0;i<24;i++) {
    bool before_stop = false;
    bool after_start = false;
    bool match = false;
    unsigned int address;
    unsigned int top;
    if (debug) {
      Serial2.print("Ch");
      Serial2.print(i);
      Serial2.print(" phi ");
      Serial2.print(phi_array[i]);
    }
    // Figure out if this channel's phi sector is before the stop
    if (phi_array[i] <= stop) before_stop = true;
    // Figure out if this channel's phi sector is after the start.
    if (phi_array[i] >= start) after_start = true;
    
    // Pay attention to wraparound:
    // If we don't wrap around 16, then you want to be after start and before stop.
    // If we do wrap around 16, then you want to be after start OR before stop.
    if (stop < start) {
     if (before_stop || after_start) match = true;
    } else {
     if (before_stop && after_start) match = true;
    }
    if (debug) {
      if (match) Serial2.println(" in range.");
      else Serial2.println(" not in range.");
    }
    // Find out this TUFF channels' address.
    address = tuff_ch_to_address(i);
    if (address & 0x4000) top = 1;
    else top = 0;
    if (i>11) {      
      if (match) on_mask[top+2] |= address;
      else off_mask[top+2] |= address;
      if (debug) {
        Serial2.print("On mask now ");
        Serial2.println(on_mask[top]);
        Serial2.print("Off mask now ");
        Serial2.println(off_mask[top]);
      }
    } else {
      if (match) on_mask[top] |= address;
      else off_mask[top] |= address;
      if (debug) {
        Serial2.print("On mask now ");
        Serial2.println(on_mask[top+2]);
        Serial2.print("Off mask now ");
        Serial2.println(off_mask[top+2]);
      }
    }
  }
}

void parseJsonCommand() {
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(cmd_buffer);
  if (!root.success()) {
      if (debug) Serial2.println("invalid JSON received");
      return;
  }
  if (root.containsKey("debug")) {
    unsigned char val = root["debug"];
    if (val) {
      Serial2.begin(115200);
      Serial2.print("iRFCM ");
      Serial2.print(irfcm);
      Serial2.println(" beginning debug.");
      debug = true;
    } else {
      if (debug) {
        Serial2.print("iRFCM ");
        Serial2.print(irfcm);
        Serial2.println(" ending debug.");
        Serial2.end();
      }        
      debug = false;
    }
  }
  if (root.containsKey("quiet")) {
    unsigned char val = root["quiet"];
    if (val) {
      if (debug) Serial2.println("Going quiet.");
      quiet = true;
    } else {
      if (debug) Serial2.println("Going loud.");
      quiet = false;
    }
  }
  if (root.containsKey("reset")) {
    unsigned char val = root["reset"];
    if (val == irfcm) {
        if (debug) Serial2.println("Resetting TUFFs.");
        resetAll();
        sendAck();
    }
  }
  // Ping commands.
  if (root.containsKey("ping")) {
    JsonArray& pingArray = root["ping"];
    for (size_t i=0;i<pingArray.size();i++) {
      unsigned int irfcm_to_ping;
      irfcm_to_ping = pingArray[i];
      if (irfcm_to_ping == irfcm) {
        led_blink_on(TX_LED);
        if (debug) Serial2.println("Got ping, returning pong.");
        Serial.print("{\"pong\":");
        Serial.print(irfcm);
        Serial.println("}");
      }      
    }
  }    
  // Notch Range commands DO NOT ACK.
  if (root.containsKey("r0")) {
    JsonArray& phiarr = root["r0"];
    unsigned int start = phiarr[0];
    unsigned int stop = phiarr[1];

    notch_range_command(0x1, start, stop);
  }
  if (root.containsKey("r1")) {
    JsonArray& phiarr = root["r1"];
    unsigned int start = phiarr[0];
    unsigned int stop = phiarr[1];
    
    notch_range_command(0x2, start, stop);
  }
  if (root.containsKey("r2")) {
    JsonArray& phiarr = root["r2"];
    unsigned int start = phiarr[0];
    unsigned int stop = phiarr[1];

    notch_range_command(0x4, start, stop);
  }

  if (root.containsKey("test")) {
    JsonArray &testArray = root["test"];
    if (testArray[0] == irfcm) {
      unsigned int tuff = testArray[1];
      if (tuff) {
        tuff2.end();
        // do something more.
        pinMode(tuff_clock[1], OUTPUT);
        digitalWrite(tuff_clock[1], 0);
        digitalWrite(tuff_reset[1], RESET_ACTIVE);
        digitalWrite(tuff_reset[1], RESET_INACTIVE);
        delay(10);
        digitalWrite(tuff_clock[1], 1);        
        tuff2.begin();
        tuff2.setDataMode(SPI_MODE3);
        tuff2.setClockDivider(16);        
      } else {
        tuff0.end();
        // do something more.
        pinMode(tuff_clock[0], OUTPUT);
        digitalWrite(tuff_clock[0], 0);
        digitalWrite(tuff_reset[0], RESET_ACTIVE);
        digitalWrite(tuff_reset[0], RESET_INACTIVE);
        delay(10);
        digitalWrite(tuff_clock[0], 1);        
        tuff0.begin();
        tuff0.setDataMode(SPI_MODE3);
        tuff0.setClockDivider(16);        
      }
      sendAck();
    }
  }
      
  // On commands.
  if (root.containsKey("on")) {
    JsonArray& onArray = root["on"];
    if (onArray[0] == irfcm) {
      unsigned int tuff = tuff_ch_to_tuff(onArray[1]);
      unsigned int addr = tuff_ch_to_address(onArray[1]);
      unsigned int mask = onArray[2];
      mask = mask & 0x7;
      // Add the notch command, the mask, and turn those on.
      addr = addr | 0x80 | (mask << 3) | mask;
      tuffCommand(tuff, addr);
      sendAck();
    }
  }
  // Off commands.
  if (root.containsKey("off")) {
    JsonArray& offArray = root["off"];
    if (offArray[0] == irfcm) {
      unsigned int tuff = tuff_ch_to_tuff(offArray[1]);
      unsigned int addr = tuff_ch_to_address(offArray[1]);
      unsigned int mask = offArray[2];
      mask = mask & 0x7;
      // Add the notch command, the mask, and turn those off.
      addr = addr | 0x80 | (mask << 3);
      tuffCommand(tuff, addr);
      sendAck();
    }
  }
  // Raw commands. 
  if (root.containsKey("raw")) {
    JsonArray& rawArray = root["raw"];
    if (rawArray[0] == irfcm) {
      unsigned int tuff = rawArray[1];
      unsigned int cmd = rawArray[2];
      tuffCommand(tuff, cmd);
      sendAck();
    }
  }
  if (root.containsKey("monitor")) {
    unsigned int target = root["monitor"];
    if (target == irfcm) {
      uint32_t TempRead;
      float TempC;
      TempRead = analogRead(TEMPSENSOR);
      TempC = (float)(1475 - ((2475*TempRead)/4096))/10;
      Serial.print("{\"temp\":");
      Serial.print(TempC);
      Serial.println("}");
    }
  }
  // Set commands.
  if (root.containsKey("set")) {
    JsonObject& set = root["set"];
    // Check if this is an iRFCM specific set.
    // If so, only handle it if this is us.
    if (set.containsKey("irfcm")) {
      unsigned int target = set["irfcm"];
      if (target == irfcm) {
        if (set.containsKey("default")) {
          JsonArray& defaultArray = set["default"];
          uint16_t defaultNum = defaultArray[0];
          int defaultVal = defaultArray[1];
          if (defaultNum < 4) {
            if (defaultVal < 0) {
              // disable it
              tuff_default_array[4] |= (1<<defaultNum);
            } else {
              tuff_default_array[4] &= ~(1<<defaultNum);              
              tuff_default_array[defaultNum] = defaultVal;
            }
            ROM_EEPROMProgram(tuff_default_array, sizeof(irfcm)+sizeof(phi_array), sizeof(tuff_default_array));
            sendAck();
          }
        }
        if (set.containsKey("addr")) {          
          unsigned int target = set["addr"];
          unsigned int val;
          if (set.containsKey("save")) {
            val = set["save"];
          } else {
            val = 1;
          }
          updateiRFCM(target, val);
        }
        if (set.containsKey("phi")) {
          unsigned int saveVal;
          JsonArray& phiArray = set["phi"];
          for (size_t i=0;i<phiArray.size();i++) {
            phi_array[i] = phiArray[i];
          }
          if (set.containsKey("save")) {
            unsigned int val;
            saveVal = set["save"];
          } else {
            saveVal = 1;
          }
          if (saveVal) {
            ROM_EEPROMProgram(phi_array, sizeof(irfcm), sizeof(phi_array));
          }
          sendAck();
        }        
      }
    } else if (set.containsKey("addr")) {
      unsigned int target = set["addr"];
      unsigned int val;
      if (set.containsKey("save")) {
        val = set["save"];
      } else {
        val = 1;
      }
      updateiRFCM(target, val);
    }
  }
  if (root.containsKey("bootload")) {
    unsigned int key = root["bootload"];
    if (key == 12345) {
      // The bootloader has a 47k pullup, and an 0.1 uF capacitor.
      // For a normal power-on (generated by a BOR), the chip is held in reset for ~10 ms.
      // Therefore the bootloader will be high by then.
      // However, we're going to drive the pin low, and then issue a SysCtlReset().
      // This reset is much quicker (~microseconds) so the pin will still be low by then.
      // An 0.1 uF cap at 3.3V holds 0.33 uC of charge, so bleeding it off at 2 mA
      // will take around 165 us. So let's sleep a bit.
      pinMode(2, OUTPUT);
      digitalWrite(2, 0);
      delay(2);
      // And now reset.
      SysCtlReset();
    }
  }
}

void notch_range_command(unsigned int notch, unsigned int start, unsigned int stop) {
  unsigned short on_mask[4] = { 0, 0, 0, 0};
  unsigned short off_mask[4] = { 0, 0, 0, 0};
  
  build_mask(start, stop, on_mask, off_mask);
  for (unsigned int i=0;i<4;i++) {
    // OK, so now we have our address masks. Add the notch command (0x80), the notch mask (0x1<<3), and notch on (0x1).
    on_mask[i] |= 0x80 | (notch<<3) | notch;
    // Add notch command, and notch mask (no notch on).
    off_mask[i] |= 0x80 | (notch<<3);
  }
  if (debug) {
    Serial2.print("Ranged notch ");
    Serial2.print(notch);
    Serial2.print(":");
    Serial2.print(start);
    Serial2.print("-");
    Serial2.println(stop);
    Serial2.print("On Commands: ");
    Serial2.print(on_mask[0], HEX);
    Serial2.print(" ");
    Serial2.print(on_mask[1], HEX);
    Serial2.print(" ");
    Serial2.print(on_mask[2], HEX);
    Serial2.print(" ");
    Serial2.println(on_mask[3], HEX);
    Serial2.print("Off Commands:  ");
    Serial2.print(off_mask[0], HEX);
    Serial2.print(" ");
    Serial2.print(off_mask[1], HEX);
    Serial2.print(" ");
    Serial2.print(off_mask[2], HEX);
    Serial2.print(" ");
    Serial2.println(off_mask[3], HEX);
  }

  // Go 0,2,1,3 here to allow the 0/2 stacks to proceed in parallel.
  // Turn on.
  tuffCommand(0, on_mask[0]);
  tuffCommand(1, on_mask[2]);
  tuffCommand(0, on_mask[1]);
  tuffCommand(1, on_mask[3]);
  // Turn off.
  tuffCommand(0, off_mask[0]);
  tuffCommand(1, off_mask[2]);
  tuffCommand(0, off_mask[1]);
  tuffCommand(1, off_mask[3]);
}

void updateiRFCM(unsigned int target, unsigned int save) {
  // 42 is a magic iRFCM: it can't be reassigned.
  if (irfcm != 42) {
    irfcm = target;
    if (save) {
      ROM_EEPROMProgram(&irfcm, 0, sizeof(irfcm));          
    }
    sendAck();
  }
}

void sendAck() {
    if (!quiet) {
      led_blink_on(TX_LED);
      Serial.print("{\"ack\":");
      Serial.print(irfcm);
      Serial.println("}");
    }
}  
