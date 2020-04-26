
# What is this?

This is a proof-of-concept (but verified working) assembly
to create a projector dowser. It is controllable via DMX for
theatrical use, though has not (yet) been tested in a live
production.

This is a personal project and no warrantee or guarantee of any
kind is given. Use at your own risk.

A dowser can be built for under $100 with this approach, and
with some more work more cost can be shaved off. Commercially
versions typically run for $100 or more, though you likely
get what you pay for.

You will need a 3D Printer and assorted off-the-shelf parts. If you
want to edit the CAD files directly, you'll also need a recent copy
of Vectorworks, though you may be able to import the files into
other CAD software.

![Photo](...)



# Parts List

- Arduino Uno
  - https://store.arduino.cc/usa/arduino-uno-rev3

- DMX Shield for Arduino - Isolated (CTC-DRA-10-r2)
  - https://www.tindie.com/products/Conceptinetics/25kv-isolated-dmx-512-shield-for-arduino-r2/

- LCD Keypad Shield (e.g., DFR0009)*
  - Note: for button layout try to find the older version

- Servo Motor (e.g., HS-425BB)
  - https://www.amazon.com/Hitec-RCD-31425S-HS-425BB-Servo/dp/B0006O3WXM

- Stepdown Voltage Converter with barrel connector
  - https://www.amazon.com/gp/product/B01NALDSJ0

- Dowser Flag, premade (or make your own):
  - https://www.stagelightingstore.com/home/79056-city-theatrical-dowser-flag
  
- Pin Extenders, assorted**
  - https://www.amazon.com/Shield-Stacking-Header-Arduino-Pack/dp/B0756KRCFX/



*There seem to be different revisions of this out there. You may need to make
slight adjustments to the Arduino code and/or the 3d printed case top if you
have a different revision.

**for prototyping; a more permanent solution is recommended if you're going do to
this for serious work.
