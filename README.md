# Summary

This is the Windows command line tool for Prog, a programmer for selected Microchip PIC16F, PIC18F and PIC32F microcomputers. Prog-Win reads [HEX files](https://en.wikipedia.org/wiki/Intel_HEX) and communicates with the programmer hardware (a PICF2550 with the [prog-25k50](https://github.com/jhbe/prog-25k50) firmware) over USB. Typically the HEX file would have been generated by one of the (free) Microchip compilers.

    +----------------+       +-------------+        +------------------------------+
    | Windows PC     |       | Programmer  |--Vdd-->| PIC16/18/32 to be programmed |
    |                |       |             |--Gnd-->|                              |
    |  prog-win.exe <---USB->| PIC25K50    |<-PGD-->|                              |
    |                |       | MAX680      |--PGC-->|                              |
    +----------------+       +-------------+  etc.. +------------------------------+

Note that the picture above illustrates the Prog chicken-and-egg problem; without access to a preprogrammed PIC25K50 you need the programmer to program the programmer PIC.

# Build

The repo is configured for Eclipse Neon with a MingW-64 compiler chain. MingW provides the windows.h and Usb100.h header file as well as the setupapi and winusb libraries. A successful build would look like this:

    g++ -O0 -g3 -Wall -c -fmessage-length=0 -o Pic18.o "..\\Pic18.cpp" 
    g++ -O0 -g3 -Wall -c -fmessage-length=0 -o Pic16.o "..\\Pic16.cpp" 
    g++ -O0 -g3 -Wall -c -fmessage-length=0 -o Usb.o "..\\Usb.cpp" 
    g++ -O0 -g3 -Wall -c -fmessage-length=0 -o Prog.o "..\\Prog.cpp" 
    g++ -O0 -g3 -Wall -c -fmessage-length=0 -o Pic32.o "..\\Pic32.cpp" 
    g++ -o Prog-Win.exe HexFile.o Pic16.o Pic18.o Pic32.o Prog.o Usb.o -lsetupapi -lwinusb 

# Verification

If the programmer hardware has been connected and drivers successfully loaded, then the command

    Prog-Win.exe -16

should print nothing. If it says

    *** SetupDiEnumDeviceInterfaces failed
    *** Failed to open the USB connection to the programmer.

then Windows has not recognised the programmer as a functional USB device. See the [prog-2550 readme](https://github.com/jhbe/prog-2550) for further details and troubleshooting.

# Usage

Replace the "-16" used in the examples below with "-18" or "-32" for PIC18F and PIC32F devices.

Get help:

    Prog-Win.exe -?

Print the contents of a hex file in readable format:

    Prog-Win.exe -h my_hex_file.hex

Verify that the programmer can detect the chip to be programmed:

    Prog-Win.exe -16 -id

Erase and program:

    Prog-Win.exe -16 -e -p my_hex_file.hex

Dump the contents of selected memory areas:

    Prog-Win.exe -16 -d

# Limitations

There is currently no support for programming EEPROM variables (read: semi-static RAM). There's no technical reason for why it could not be added, I just never needed it.

# Known Issues

Sometimes, and in particular with long (>100mm) leads between the programmer and the chip to be programmed, the verification step may fail. Another try often helps. Shorter leads does too.

# TODO

* One failed verification is enough. Stop on first fail and do not print "Programmed!" at the end.
* Support for EEPROM variables.
