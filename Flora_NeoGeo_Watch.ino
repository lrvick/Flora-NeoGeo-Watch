#include <Adafruit_GPS.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LSM303.h>
#include <Time.h>
#include "config.h"

Adafruit_GPS GPS(&Serial1);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, 6, NEO_GRB + NEO_KHZ800);
LSM303 compass;

float targetLat = GEO_LAT;
float targetLon = GEO_LON;
float tripDistance;
const int utcOffset = UTC_OFFSET;
int topLED = TOP_LED;
int compassOffset = LED_OFFSET;
int brightness = BRIGHTNESS;
int lastMin = 16;
int lastHour = 16;
int lastCombined = 0;
int startLED = 0;
int startLEDlast = 16;
int start = 0;
int mode = 0;
int lastDir = 16;
int dirLED_r = 0;
int dirLED_g = 0;
int dirLED_b = 255;
int compassReading;
int buttonPin = 10;            // the number of the pushbutton pin
int buttonState;               // the current reading from the input pin
int lastButtonState = HIGH;    // the previous reading from the input pin
long buttonHoldTime = 0;       // the last time the output pin was toggled
long buttonHoldDelay = BUTTON_HOLD_DELAY;

float fLat = 0.0;
float fLon = 0.0;

uint32_t gpsTimer = millis();
uint32_t startupTimer = millis();
uint32_t compassTimer = millis();

void setup() {

  strip.setBrightness(brightness);

  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(115200);

  GPS.begin(9600);
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  compass.init();
  compass.enableDefault();
  compass.m_min.x = COMPASS_MIN_X;
  compass.m_min.y = COMPASS_MIN_Y;
  compass.m_min.z = COMPASS_MIN_Z;
  compass.m_max.x = COMPASS_MAX_X;
  compass.m_max.y = COMPASS_MAX_Y;
  compass.m_max.z = COMPASS_MAX_Z;

  delay(1000);
  // Ask for firmware version
  Serial1.println(PMTK_Q_RELEASE);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Make input & enable pull-up resistors on switch pins for pushbutton
  pinMode(buttonPin, INPUT);
  digitalWrite(buttonPin, HIGH);

}


void loop() {

  compassCheck();
  // read the state of the switch into a local variable:
  int buttonState = digitalRead(buttonPin);

  if (buttonState == LOW) {
    buttonCheck();
  }

  lastButtonState = buttonState;

  //Serial.println(buttonState);
  // read data from the GPS in the 'main loop'
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  if (VERBOSE)
    if (c) Serial.print(c);
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }

  // if millis() or timer wraps around, we'll just reset it
  if (gpsTimer > millis()) gpsTimer = millis();

  if (start == 0) {
    if (GPS.fix) {
      // set the Time to the latest GPS reading
      setTime(GPS.hour, GPS.minute, GPS.seconds, GPS.day, GPS.month, GPS.year);
      delay(50);
      adjustTime(utcOffset * SECS_PER_HOUR);
      delay(500);
      tripDistance = (double)calc_dist(fLat, fLon, targetLat, targetLon);
      start = 1;
    }
  }
  // approximately every 60 seconds or so, update time
  if ((millis() - gpsTimer > 60000) && (start == 1)) {
    gpsTimer = millis(); // reset the timer
    if (GPS.fix) {
      // set the Time to the latest GPS reading
      setTime(GPS.hour, GPS.minute, GPS.seconds, GPS.day, GPS.month, GPS.year);
      delay(50);
      adjustTime(utcOffset * SECS_PER_HOUR);
      delay(500);
    }
  }

  if (GPS.fix) {
    fLat = decimalDegrees(GPS.latitude, GPS.lat);
    fLon = decimalDegrees(GPS.longitude, GPS.lon);
  }

  if (mode == 0) {
    clockMode();
  }

  if (mode == 1) {
    navMode();
  }

  if (mode == 2) {
    compassMode();
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

void buttonCheck() {
  int buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && lastButtonState == HIGH) {
    buttonHoldTime = millis();
  }

  if (buttonState == LOW && lastButtonState == LOW) {
    if ((millis() - buttonHoldTime) > buttonHoldDelay) {

      if(mode == 2) {
        mode = 0;
        lastMin = 16;
        lastHour = 16;
      } else {
        mode = mode + 1;
      }
      colorWipe(strip.Color(0, 0, 0), 20);
      buttonHoldTime = millis();
    }
  }
}
uint32_t timer = millis();

void clockMode() {
if (start == 1) {
    strip.setPixelColor(startLEDlast, strip.Color(0, 0, 0));
    strip.show();

  if (millis() - timer > 2000) {
    timer = millis(); // reset the timer
    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: "); Serial.println((int)GPS.fixquality);
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", ");
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);

      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.print("Satellites: "); Serial.println((int)GPS.satellites);
    }
  }



    float gpsMin = (minute() + (second()/60.0));
    unsigned int ledMin = 0;
    int minTemp = 0;
    if (gpsMin <= 1.875) minTemp = topLED;
    if ((gpsMin > 1.875) && (gpsMin <= 5.625)) minTemp = topLED - 1;
    if ((gpsMin > 5.625) && (gpsMin <= 9.375)) minTemp = topLED - 2;
    if ((gpsMin > 9.375) && (gpsMin <= 13.125)) minTemp = topLED - 3;
    if ((gpsMin > 13.125) && (gpsMin <= 16.875)) minTemp = topLED - 4;
    if ((gpsMin > 16.875) && (gpsMin <= 20.625)) minTemp = topLED - 5;
    if ((gpsMin > 20.625) && (gpsMin <= 24.375)) minTemp = topLED - 6;
    if ((gpsMin > 24.375) && (gpsMin <= 28.125)) minTemp = topLED - 7;
    if ((gpsMin > 28.125) && (gpsMin <= 31.875)) minTemp = topLED - 8;
    if ((gpsMin > 31.875) && (gpsMin <= 35.625)) minTemp = topLED - 9;
    if ((gpsMin > 35.625) && (gpsMin <= 39.375)) minTemp = topLED - 10;
    if ((gpsMin > 39.375) && (gpsMin <= 43.125)) minTemp = topLED - 11;
    if ((gpsMin > 43.125) && (gpsMin <= 46.875)) minTemp = topLED - 12;
    if ((gpsMin > 46.875) && (gpsMin <= 50.625)) minTemp = topLED - 13;
    if ((gpsMin > 50.625) && (gpsMin <= 54.375)) minTemp = topLED - 14;
    if ((gpsMin > 54.375) && (gpsMin <= 58.125)) minTemp = topLED - 15;
    if (gpsMin > 58.125) minTemp = topLED - 16;

    if (minTemp < 0) {
      ledMin = minTemp + 16;
    } else {
      ledMin = minTemp;
    }

    float gpsHour = (hour() + (minute()/60.0));
    if (gpsHour > 12) { gpsHour = gpsHour - 12; }
    unsigned int ledHour = 0;
    int hourTemp = 0;
    if (gpsHour <= 0.375) hourTemp = topLED;
    if ((gpsHour > 0.375) && (gpsHour <= 1.125)) hourTemp = topLED - 1;
    if ((gpsHour > 1.125) && (gpsHour <= 1.875)) hourTemp = topLED - 2;
    if ((gpsHour > 1.875) && (gpsHour <= 2.625)) hourTemp = topLED - 3;
    if ((gpsHour > 2.625) && (gpsHour <= 3.375)) hourTemp = topLED - 4;
    if ((gpsHour > 3.375) && (gpsHour <= 4.125)) hourTemp = topLED - 5;
    if ((gpsHour > 4.125) && (gpsHour <= 4.875)) hourTemp = topLED - 6;
    if ((gpsHour > 4.875) && (gpsHour <= 5.625)) hourTemp = topLED - 7;
    if ((gpsHour > 5.625) && (gpsHour <= 6.375)) hourTemp = topLED - 8;
    if ((gpsHour > 6.375) && (gpsHour <= 7.125)) hourTemp = topLED - 9;
    if ((gpsHour > 7.125) && (gpsHour <= 7.875)) hourTemp = topLED - 10;
    if ((gpsHour > 7.875) && (gpsHour <= 8.625)) hourTemp = topLED - 11;
    if ((gpsHour > 8.625) && (gpsHour <= 9.375)) hourTemp = topLED - 12;
    if ((gpsHour > 9.375) && (gpsHour <= 10.125)) hourTemp = topLED - 13;
    if ((gpsHour > 10.125) && (gpsHour <= 10.875)) hourTemp = topLED - 14;
    if ((gpsHour > 10.875) && (gpsHour <= 11.625)) hourTemp = topLED - 15;
    if (gpsHour > 11.625) hourTemp = topLED - 16;

    if (hourTemp < 0) {
      ledHour = hourTemp + 16;
    } else {
      ledHour = hourTemp;
    }

    if ((ledHour == ledMin) && (lastCombined == 0)) {
      strip.setPixelColor(lastHour, strip.Color(0, 0, 0));
      strip.setPixelColor(lastMin, strip.Color(0, 0, 0));
      strip.setPixelColor(ledHour, strip.Color(255, 0, 255));
      strip.show();
      lastCombined = 1;
      lastHour = ledHour;
      lastMin = ledMin;
    } else {
      if (lastHour != ledHour) {
        strip.setPixelColor(lastHour, strip.Color(0, 0, 0));
        strip.setPixelColor(ledHour, strip.Color(255, 50, 0));
        strip.show();
        lastHour = ledHour;
      }
      if (lastMin != ledMin) {
        strip.setPixelColor(lastMin, strip.Color(0, 0, 0));
        strip.setPixelColor(ledMin, strip.Color(200, 200, 0));
        if (lastCombined == 1) {
          strip.setPixelColor(ledHour, strip.Color(255, 0, 0));
          lastCombined = 0;
        }
        strip.show();
        lastMin = ledMin;
      }
    }
  } else {
    // if millis() or timer wraps around, we'll just reset it
    if (startupTimer > millis()) startupTimer = millis();

    // approximately every 10 seconds or so, update time
    if (millis() - startupTimer > 200) {
      startupTimer = millis(); // reset the timer
      if (startLED == 16) {
        startLED = 0;
      }
      strip.setPixelColor(startLEDlast, strip.Color(0, 0, 0));
      strip.setPixelColor(startLED, strip.Color(0, 255, 0));
      strip.show();
      startLEDlast = startLED;
      startLED++;
      //delay(200);
    }
  }
}

void navMode() {
  if (start == 1) {

  compassCheck();

  headingDistance((double)calc_dist(fLat, fLon, targetLat, targetLon));

  if ((calc_bearing(fLat, fLon, targetLat, targetLon) - compassReading) > 0) {
    compassDirection(calc_bearing(fLat, fLon, targetLat, targetLon)-compassReading);
  } else {
    compassDirection(calc_bearing(fLat, fLon, targetLat, targetLon)-compassReading+360);
  }

  } else {
    // if millis() or timer wraps around, we'll just reset it
    if (startupTimer > millis()) startupTimer = millis();

    // approximately every 10 seconds or so, update time
    if (millis() - startupTimer > 200) {
      startupTimer = millis(); // reset the timer
      if (startLED == 16) {
        startLED = 0;
      }
      strip.setPixelColor(startLEDlast, strip.Color(0, 0, 0));
      strip.setPixelColor(startLED, strip.Color(0, 0, 255));
      strip.show();
      startLEDlast = startLED;
      startLED++;
    }
  }
}

int calc_bearing(float flat1, float flon1, float flat2, float flon2) {

  float calc;
  float bear_calc;

  float x = 69.1 * (flat2 - flat1);
  float y = 69.1 * (flon2 - flon1) * cos(flat1/57.3);

  calc=atan2(y,x);

  bear_calc= degrees(calc);

  if(bear_calc<=1){
    bear_calc=360+bear_calc;
  }
  return bear_calc;
}

void headingDistance(int fDist) {

  //Use this part of the code to determine how far you are away from the destination.
  //The total trip distance (from where you started) is divided into five trip segments.
  float tripSegment = tripDistance/5;

  if (fDist >= (tripSegment*4)) {
    dirLED_r = 255;
    dirLED_g = 0;
    dirLED_b = 0;
  }

  if ((fDist >= (tripSegment*3))&&(fDist < (tripSegment*4))) {
    dirLED_r = 255;
    dirLED_g = 0;
    dirLED_b = 0;
  }

  if ((fDist >= (tripSegment*2))&&(fDist < (tripSegment*3))) {
    dirLED_r = 255;
    dirLED_g = 255;
    dirLED_b = 0;
  }

  if ((fDist >= tripSegment)&&(fDist < (tripSegment*2))) {
    dirLED_r = 255;
    dirLED_g = 255;
    dirLED_b = 0;
  }

  if ((fDist >= 5)&&(fDist < tripSegment)) {
    dirLED_r = 255;
    dirLED_g = 255;
    dirLED_b = 0;
  }

  if ((fDist < 5)) { // You are now within 5 meters of your destination.
    //Serial.println("Arrived at destination!");
    dirLED_r = 0;
    dirLED_g = 255;
    dirLED_b = 0;
  }
}


unsigned long calc_dist(float flat1, float flon1, float flat2, float flon2) {

  float dist_calc=0;
  float dist_calc2=0;
  float diflat=0;
  float diflon=0;

  diflat=radians(flat2-flat1);
  flat1=radians(flat1);
  flat2=radians(flat2);
  diflon=radians((flon2)-(flon1));

  dist_calc = (sin(diflat/2.0)*sin(diflat/2.0));
  dist_calc2= cos(flat1);
  dist_calc2*=cos(flat2);
  dist_calc2*=sin(diflon/2.0);
  dist_calc2*=sin(diflon/2.0);
  dist_calc +=dist_calc2;

  dist_calc=(2*atan2(sqrt(dist_calc),sqrt(1.0-dist_calc)));

  dist_calc*=6371000.0; //Converting to meters
  return dist_calc;

}

// Convert NMEA coordinate to decimal degrees
float decimalDegrees(float nmeaCoord, char dir) {
  uint16_t wholeDegrees = 0.01*nmeaCoord;
  int modifier = 1;

  if (dir == 'W' || dir == 'S') {
    modifier = -1;
  }

  return (wholeDegrees + (nmeaCoord - 100.0*wholeDegrees)/60.0) * modifier;
}

void compassMode() {
    dirLED_r = 0;
    dirLED_g = 0;
    dirLED_b = 255;
    compassDirection(compassReading);
}

void compassCheck() {
// if millis() or timer wraps around, we'll just reset it
  if (compassTimer > millis()) compassTimer = millis();

  // approximately every 10 seconds or so, update time
  if (millis() - compassTimer > 50) {
    compassTimer = millis(); // reset the timer
    compass.read();
    compassReading = compass.heading((LSM303::vector){0,-1,0});
    }
}

void compassDirection(int compassHeading) {

  //Serial.print("Compass Direction: ");
  //Serial.println(compassHeading);

  unsigned int ledDir = 2;
  int tempDir = 0;
  //Use this part of the code to determine which way you need to go.
  //Remember: this is not the direction you are heading, it is the direction to the destination (north = forward).
  if ((compassHeading > 348.75)||(compassHeading < 11.25)) {
    //Serial.println("  N");
    //Serial.println("Forward");
    if (mode == 1 ) {
      tempDir = topLED;
    } else {
      tempDir = topLED;
    }
  }

  if ((compassHeading >= 11.25)&&(compassHeading < 33.75)) {
    //Serial.println("NNE");
    //Serial.println("Go Right");
    if (mode == 1 ) {
      tempDir = topLED - 1;
    } else {
      tempDir = topLED + 1;
    }
  }

  if ((compassHeading >= 33.75)&&(compassHeading < 56.25)) {
    //Serial.println(" NE");
    //Serial.println("Go Right");
    if (mode == 1 ) {
      tempDir = topLED - 2;
    } else {
      tempDir = topLED + 2;
    }
  }

  if ((compassHeading >= 56.25)&&(compassHeading < 78.75)) {
    //Serial.println("ENE");
    //Serial.println("Go Right");
    if (mode == 1 ) {
      tempDir = topLED - 3;
    } else {
      tempDir = topLED + 3;
    }
  }

  if ((compassHeading >= 78.75)&&(compassHeading < 101.25)) {
    //Serial.println("  E");
    //Serial.println("Go Right");
    if (mode == 1 ) {
      tempDir = topLED - 4;
    } else {
      tempDir = topLED + 4;
    }
  }

  if ((compassHeading >= 101.25)&&(compassHeading < 123.75)) {
    //Serial.println("ESE");
    //Serial.println("Go Right");
    if (mode == 1 ) {
      tempDir = topLED - 5;
    } else {
      tempDir = topLED + 5;
    }
  }

  if ((compassHeading >= 123.75)&&(compassHeading < 146.25)) {
    //Serial.println(" SE");
    //Serial.println("Go Right");
    if (mode == 1 ) {
      tempDir = topLED - 6;
    } else {
      tempDir = topLED + 6;
    }
  }

  if ((compassHeading >= 146.25)&&(compassHeading < 168.75)) {
    //Serial.println("SSE");
    //Serial.println("Go Right");
    if (mode == 1 ) {
      tempDir = topLED - 7;
    } else {
      tempDir = topLED + 7;
    }
  }

  if ((compassHeading >= 168.75)&&(compassHeading < 191.25)) {
    //Serial.println("  S");
    //Serial.println("Turn Around");
    if (mode == 1 ) {
      tempDir = topLED - 8;
    } else {
      tempDir = topLED + 8;
    }
  }

  if ((compassHeading >= 191.25)&&(compassHeading < 213.75)) {
    //Serial.println("SSW");
    //Serial.println("Go Left");
    if (mode == 1 ) {
      tempDir = topLED - 9;
    } else {
      tempDir = topLED + 9;
    }
  }

  if ((compassHeading >= 213.75)&&(compassHeading < 236.25)) {
    //Serial.println(" SW");
    //Serial.println("Go Left");
    if (mode == 1 ) {
      tempDir = topLED - 10;
    } else {
      tempDir = topLED + 10;
    }
  }

  if ((compassHeading >= 236.25)&&(compassHeading < 258.75)) {
    //Serial.println("WSW");
    //Serial.println("Go Left");
    if (mode == 1 ) {
      tempDir = topLED - 11;
    } else {
      tempDir = topLED + 11;
    }
  }

  if ((compassHeading >= 258.75)&&(compassHeading < 281.25)) {
    //Serial.println("  W");
    //Serial.println("Go Left");
    if (mode == 1 ) {
      tempDir = topLED - 12;
    } else {
      tempDir = topLED + 12;
    }
  }

  if ((compassHeading >= 281.25)&&(compassHeading < 303.75)) {
    //Serial.println("WNW");
    //Serial.println("Go Left");
    if (mode == 1 ) {
      tempDir = topLED - 13;
    } else {
      tempDir = topLED + 13;
    }
  }

  if ((compassHeading >= 303.75)&&(compassHeading < 326.25)) {
    //Serial.println(" NW");
    //Serial.println("Go Left");
    if (mode == 1 ) {
      tempDir = topLED - 14;
    } else {
      tempDir = topLED + 14;
    }
  }

  if ((compassHeading >= 326.25)&&(compassHeading < 348.75)) {
    //Serial.println("NWN");
    //Serial.println("Go Left");
    if (mode == 1 ) {
      tempDir = topLED - 15;
    } else {
      tempDir = topLED + 15;
    }
  }

  if (tempDir > 15) {
      ledDir = tempDir - 16;
    } else {
      ledDir = tempDir;
  }

  if (tempDir < 0) {
      ledDir = tempDir + 16;
    } else {
      ledDir = tempDir;
  }

  if (mode == 1) {
   ledDir = ledDir + compassOffset;
   if (ledDir > 15) {
     ledDir = ledDir - 16;
   }
  } else {
   ledDir = ledDir + compassOffset;
   if (ledDir > 15) {
     ledDir = ledDir - 16;
   }
  }

  if (lastDir != ledDir) {
    strip.setPixelColor(lastDir, strip.Color(0, 0, 0));
    strip.setPixelColor(ledDir, strip.Color(dirLED_r, dirLED_g, dirLED_b));
    strip.show();
    lastDir = ledDir;
  }

}
