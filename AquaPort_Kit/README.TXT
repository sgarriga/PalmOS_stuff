=======================================================================
AquaPort V3.0 : Copyright (c) 2001 Stephen Garriga. 

DISCLAIMER
==========
AquaPort V3.0 ("the application") is provided AS IS, with no guarantee
of fitness for any particular task either express or implied. The user
assumes any and all risks associated with running this application, or
parts thereof.


License
=======
This application is SHAREWARE, it is NOT public domain. It remains the
property of the copyright holder.

The copyright holder grants license to duplicate and distribute the 
application for evaluation purposes provided it is distributed intact
and with this readme.txt file.

The copyright holder grants license to install and evaluate the 
application for a period of not more than 30 days. If, after 30 days,
you ("the user") wish to continue use of the application, please make
a donation of at least $5 (or local equivalent) to one of the following
organizations: 
	- The Ocean Conservancy   (www.cmc-ocean.org)
        - The Coral Reef Alliance (www.coral.org)
	- New England Aquarium    (www.neaq.org)
	- World Wildlife Fund     (www.wwf.org)

By installing this Product on you PDA you are agreeing to be bound by
the terms and conditions of this license agreement. If you do not 
wish to accept this agreement, you may not install or use this Product. 

=======================================================================


About
=====
AquaPort is a PalmOS 3.1+ application, with a Windows Conduit, to 
allow  owners of Citizen Hyper Aqualand watches who use the Citizen 
'AQUALAND AQUAGRAPH2' software to download dive logs via their PalmOS 
device.

>> The PalmOS portion allows the user to download* (and preview) up to 
   50 logs (memory permitting) 
   - allowing for an extended trip without the risk of taking a Laptop
     with you.

>> The Conduit creates a set of files in format that may be imported 
   [Option>Import] into the AQUALAND GRAPH2 software. The location of 
   the files is configurable.

*(A Markspace DCH-M25 cable is required to connect a Handspring Visor 
 to the watch cradle, 'http://www.markspace.com/datacord.html'. On a 
 regular Palm a standard modem cable should suffice)


For a more general solution, have a look at Gary Lorensen's 
'DiveComputer' http://members.aol.com/Glorensen/divecomp.html


Installing
==========
Unzip the archive (if applicable).
Double click Setup.bat to install and register the conduit.
Restart HotSync.

Install AquaPort.prc using your regular install method.


Known Issues
============
1)  After a log is downloaded, the watch often goes into 
    transmission error mode. (Visor)

2)  The comms between the Handheld and Watch can be difficult, be 
    prepared to make a few attempts. (Visor)

These are mainly due to the very limited RS232 support on the Visor.

3)  If you compare an AquaPort log, to its directly downloaded 
    equivalent, There may be minor differences to interpolated 
    samples due to rounding errors. Similarly, directly downloaded 
    logs may show a different (shorter) dive time - AQUALAND GRAPH2
    appears to edit out some surface time, which AquaPort does not.

4)  Beaming is only supported while the application is active. 
    However the user is still asked if they want to receive the 
    record or not when not in the app. 
 

Compatibility
=============
This PalmOS application has been tested on:-
- a Handspring Visor Deluxe (PalmOS 3.1H3) using a Markspace DCH-M25 
  cable. 
- a Palm IIIc (PalmOS 3.5.3).

The Conduit has been tested on Win98 and WinMe machines with HotSync
Manager 3.1.0H and 3.0.2H. 

I foresee no reason why the PalmOS application wouldn't run on any 
PalmOS 3.1 or better Handheld, nor why the Conduit shouldn't work on 
a Win95, WinNT or Win2000 machine _if_ you have HotSync running 
already. However IT HASN'T BEEN TESTED - YOU ARE WARNED!


Bug Reports to : AquaPort@CyberPad.org


Revision History
================
V1.0 : Initial Working Version
v2.0 : Color Branch (Hacky & only emulator tested)
V1.1 : Several Minor Bug Fixes
v3.0 : Common Color/B&W Codebase
       Added Log Beaming


