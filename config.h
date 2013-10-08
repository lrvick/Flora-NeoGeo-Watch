// Set Latitude and longitude of desired destination
#define GEO_LAT 28.684628
#define GEO_LON -81.353095

// Your NeoPixel ring may not line up with ours
// Enter which NeoPixel led is your top LED (0-15).
#define TOP_LED 13

// Your compass module may not line up with ours.
// Once you run compass mode, compare to a separate
// compass (like one found on your smartphone)
// Point your TOP_LED north, then count clockwise
// how many LEDs away from TOP_LED the lit LED is
#define LED_OFFSET 0

// how long to hold the button down (ms)
#define BUTTON_HOLD_DELAY 500

// Offset hours from gps time (UTC)
#define UTC_OFFSET -4

// Display brightness (low values conserve battery)
#define BRIGHTNESS 2

// Serial logging (GPS data etc)
#define VERBOSE false

// Calibration values. Use the Calibrate example
// program to get the values for your compass.
#define COMPASS_MIN_X -809
#define COMPASS_MIN_Y -491
#define COMPASS_MIN_Z -793
#define COMPASS_MAX_X +451
#define COMPASS_MAX_Y +697
#define COMPASS_MAX_Z 438
