*********************************************************************
   ScanTool.net OBD-II Software v1.20 for ElmScan and OBDLink devices
   Copyright (C) 2010 ScanTool.net LLC, All Rights Reserved
*********************************************************************

======================================
============= Disclaimer =============
======================================

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version. This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
more details.

======================================
============ Introduction ============
======================================

ScanTool.net OBD-II Software for ElmScan is free software that allows you to 
use your computer and an inexpensive hardware interface to read information 
from your car's computer. Current version allows you to read trouble codes and 
see their descriptions, clear the codes and turn off the "Check Engine" light, 
and display real-time sensor data such as RPM, Engine Load, Vehicle Speed, 
Coolant Temperature, and Timing Advance.

For more information about the ElmScan OBD-II interface, please visit 
our website, located at http://www.ScanTool.net.

======================================
===== Minimum System Requirements ====
======================================

Windows:
   - 486DX 25Mhz Processor or better
   - 4Mb RAM
   - Windows 95 or higher
   - DirectX 7 or newer
   - 640x480 display
   - Serial port

DOS:
   - 386SX 10Mhz Processor or better
   - 1Mb RAM
   - DOS v3.0
   - 640x480 display
   - Serial port

======================================
========== Troubleshooting ===========
======================================

If you are having problems running the software, please visit our
Support Page located at http://www.scantool.net/support

======================================
========== Version History ===========
======================================

   v1.21  -  Made 115.2k baud rate the default
   v1.20  -  Added dynamic COM port selector (now works with COM port numbers > COM8)
             Fixed program freeze when opening Bluetooth COM ports
             Fixed potential data errors due to ELM327 UART silicone errata
             Implemented firmware version printing for new FW releases of OBDLink and OBDLink CI
             A few small bugfixes
   v1.15  -  Added support for ElmScan 5 Compact
             Added support for OBDLink and OBDLink CI devices
             Defaults can now be set by simply deleting the CFG file
             Fixed sensor pages with all sensors disabled causing other pages to delay first refresh by up to 10 seconds
             Fixed 100% CPU usage
   v1.14  -  Added 115.2k & 230.4k baud rates to options (May 23, 2008)
             Added genuine scan tool validation
   v1.13  -  Fixed some formatting, precision, and conversion errors in sensor formulas
             Fixed inconsistent refresh rate display in Sensor Data
             Refined handling of number of DTCs reported being different from number of DTCs read
             Small bug fixes
   v1.12  -  Refined error descriptions for ELM327
             CAN DTC responses are now handled properly
             SAE-defined DTCs with no description are now not identified as manufacturer-defined
             Expanded Clear Codes warning to describe all things that will be cleared
             Misc. bug fixes
   v1.11  -  Fixed a bug in Display Codes dialog, where the program would crash under some conditions
             Refined "COM Port Could not be Open" error handling
   v1.10  -  Added baud rate switching
             Made compatible with ELM327
             Added protocol detection to reset interface dialog
             Added pending DTCs
             DTCs are now read even if ECU reports 0 codes
             Added OBD Information dialog
             Cleaned up Main Menu
             Updated About dialog
             Misc. bug/cosmetic fixes
   v1.09  -  Fixed erroneous interpretation of 7F responses (KWP2000)
             Fixed number of codes reporting in Trouble Codes
             Fixed possible incorrect DTC interpretation when more than one response is received
   v1.08  -  Fixed problem with ECUs that pad the response with 0's
   v1.07  -  Added the rest of the sensors defined in SAE J1979 (APR2002)
             Added the rest of "designed to comply with" to Sensor Data
             Cleaned up portions of the code
             Added COM ports 5-8 to Options
             Updated codes.dat with latest generic P and U codes, and removed B and C codes
             System information dialog supports a wider range of processors and platforms
   v1.06  -  Fixed some problems with RS232
             Corrected air flow rate formula (US system)
             Added 16 new sensors
             Modified the layout of "Sensor Data"
             Misc. bug fixes
   v1.04  -  Updated serial library
             Some bug fixes and enhancements
             Support for multiple platforms and compilers
   v1.03  -  Fixed incorrect display of some sensors when ELM323 is used
   v1.02  -  Minor bug fixes
   v1.01  -  Minor bug fixes
   v1.00  -  Initial release
