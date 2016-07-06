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

