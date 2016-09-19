# tuff-master

Firmware for the TUFF master.

# TUFF master protocol

The TUFF master talks over a serial port at 115,200 bps, 8 bits, no parity, 1 stop bit. Commands are sent in a JSON format and are all human-readable.

# ping

Syntax: {"ping":[0,1,2,3]}

This would ping all 4 iRFCMs (TUFF masters). The list is a list of iRFCM identifiers to ping. Only iRFCMs with assigned identifiers will respond. They respond with {"pong":irfcm_number} (e.g. {"pong":0}). The order of the response gives you the order of the daisy chain.

# Notch range commands

Syntax: {"r0":[0,15]}

Commands can be "r0", "r1", or "r2", corresponding to the 3 notches (260 MHz, 380 MHz, 450 MHz). The first argument is the start phi sector (inclusive). The second argument is the stop phi sector (inclusive). So this command would turn on all of the 260 MHz notches for the instrument.

Start and stop can wrap around 16: so if it was {"r0":[15,0]} this would turn on ** only ** phi 0 and 15.

# Specific on/off commands

Syntax: {"on":[0,0,7]}

Turns on a notch for a specific channel in a specific iRFCM. First number is the iRFCM number (0,1,2,3), the second number is the channel (from 0-23), and the third is a 3-bit bitmask of the notches to turn on.

This command would turn on all 3 notches on iRFCM #0's channel 0.

Syntax: {"off":[0,0,7]}

See "on", but this turns them off.

# Advanced Commands

## raw

Syntax: {"raw":[0,1,16265]}

Sends a raw command to a TUFF. The above command would send a raw command to iRFCM 0 (0, the first parameter), to be sent to TUFF2/3 (1, the second parameter). The command
sent would be 0x3F89 (16265, the third parameter). See the tuff-slave-usi README.md for command decoding.

## debug

Syntax: {"debug":1}

Starts/stops debug mode on all iRFCMs in the chain. Debug mode puts out information on a second serial port console, which isn't useful if they're installed on an iRFCM.
So this is only useful on the bench. The parameter is debug mode on (1) or off (0).

## quiet

Syntax: {"quiet":1}

Puts all iRFCMs in the chain into quiet mode. Parameter is on (1) or off (0). Quiet mode does not ack any commands.

## monitor

Syntax: {"monitor":2}

Requests monitoring data from an iRFCM. Parameter is iRFCM that data is requested from. Response is {"temp":23.5}, which would be 23.5 C.

## reset

Syntax: {"reset":2}

Resets the TUFFs attached to iRFCM addressed (first parameter - here, 2).

## test

Syntax: {"test":[0,1]}

Puts a pair of TUFFs attached to iRFCM addressed (first parameter - here, 0) into test mode. TUFF pair addressed is second parameter (here, 1, so TUFF2/3). This hasn't really been
tested once installed.

## set

There are several set commands available, as a JSON sub-object. e.g.

* {"set":{"irfcm":0,"addr":3,"save":1}} - this would change iRFCM 0 to #3 permanently. If no 'save', change is only temporary.
* {"set":{"irfcm":0,"phi":[0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,2,2,3,3,3,3,3,3],"save":1}} - this sets iRFCM 0's phi assignments permanently.
* {"set":{"irfcm":0,"default":[0,16265]}} - this sets iRFCM 0's #0 default to 0x3F89. There are 4 defaults that can be sent: 0/1 go to TUFF01, and 2/3 go to TUFF23.

# Bootloader

Syntax: {"bootload":12345}

Places all listening masters into bootloader mode. This means that ONLY ONE iRFCM can be connected to the bootloader. So the instructions are (for a Windows computer currently):

1. Connect a SINGLE iRFCM to a serial port - no chain, or use the iRFCM 0 port.
2. Compile the firmware in Energia (click 'Verify' button)
3. Export the compiled binary. (Sketch->Export Compiled Binary).
4. Load LM Flash Programmer.
5. Find the serial port and select it. Go to the 'Program' tab, and browse to the .BIN file that was exported in the sketch directory. This is just getting ready.
6. In the Serial Monitor, type {"bootload":12345} and hit enter. Close the serial monitor.
7. In LM Flash Programmer, click Program.

There is a program to do this under Linux (sflash) but I'm still working on that.