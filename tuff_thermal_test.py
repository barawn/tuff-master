#!/usr/bin/python

import serial
import sys
import time
import io
import datetime

if len(sys.argv) < 2:
    print "Usage: tuff_thermal_test.py_port"
    sys.exit(1)

port = sys.argv[1]
print "Running thermal test on port %s" % port
errors = 0
ser = serial.Serial(port, 115200, timeout=1)
#sio = io.TextIOWrapper(io.BufferedRWPair(ser, ser))
ser.setDTR(0)
ping = "{\"ping\":[0,1,2,3]}\n"
monFmt = "{\"monitor\":%d}\n"
while True:
    print "%s : Starting loop. Pinging - " % datetime.datetime.now()
    ser.write(ping)
    line = ser.readline()
    while len(line) > 0:        
        print line.rstrip()
        line = ser.readline()
    print "%s : Ping done. Getting temp..." % datetime.datetime.now()
    for i in xrange(4):
        ser.write(monFmt % i)
        line = ser.readline()
        while len(line):
            print line.rstrip()
            line = ser.readline()
    print "%s : Temp done. Sleeping." % datetime.datetime.now()
    time.sleep(30)


