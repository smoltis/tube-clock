#include <LedControl.h>
#include <RTClib.h>
#include <Wire.h>
#include <MD_UISwitch.h>
#include <MD_YX5300.h>
#include <FastLED.h>


//************Pins*****************//
#define ARDUINO_RX 4    // connect to TX of MP3 Player module
#define ARDUINO_TX 3    // connect to RX of MP3 Player module
#define PLAY_FOLDER 1   // tracks are all placed in this folder

#define DATA_PIN            8
#define NUM_LEDS            3
#define MAX_POWER_MILLIAMPS 100
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB

const uint8_t SW_PIN[] = { 9, 10, 11 }; // Button SET MENU' // Button + // Button -
const uint8_t DIGITAL_SWITCH_ACTIVE = LOW;

//************FastLED**************//
#define FASTLED_ALLOW_INTERRUPTS 0
FASTLED_USING_NAMESPACE
CRGB leds[NUM_LEDS];

//**********7SegmentDisplay***********//
/* 
 * We use pins 7,5 and 6 on the Arduino for the SPI interface
 * Pin 7 is connected to the DATA IN-pin of the first MAX7221
 * Pin 5 is connected to the CLK-pin of the first MAX7221
 * Pin 6 is connected to the LOAD(/CS)-pin of the first MAX7221   
 * There will only be a single MAX7221 attached to the arduino 
 */
LedControl lc=LedControl(7,5,6,1);

//************XY5300MP3**************//
MD_YX5300 mp3(ARDUINO_RX, ARDUINO_TX);

//************DIGITAL_BTN**************//
MD_UISwitch_Digital *BTN[ARRAY_SIZE(SW_PIN)]; // SET, UP, DOWN 
enum buttons {
  SET,
  UP,
  DOWN,
  NONE
};

enum buttons btnCtrl = NONE;
//************RTC_DS3231**************//
RTC_DS3231 rtc;

//************Variables**************//
int hourupg;
int minupg;
int secupg;
int yearupg;
int monthupg;
int dayupg;
bool playerPause = true;  // true if MP3 player is currently paused
bool dotFlag = false;
//**********Menu FSM*********************//
enum menuStates {
  TIME, // display time
  DATE, // display date
  OPTIONS,
  SET_HOUR, // set hour
  SET_MIN, // set minute
  SET_DAY, // set minute
  SET_MONTH, // set month
  SET_YEAR, // set year
  // SET_ALARM, // set alarm
  SAVE // save settings
};
 
enum menuStates displayState;
long interval = 1000;
unsigned long previousMillis = 0;
unsigned long previousMillisTO = 0;
//************DisplayFuncs**************//
void printNum(int v, int leftPost, bool dPoint = false){
  int ones;
  int tens;
  ones=v%10;
  v=v/10;
  tens=v%10;
  lc.setDigit(0,leftPost,(byte)tens,false);
  lc.setDigit(0,leftPost - 1,(byte)ones, dPoint);
}

void printChar(char c, int digit, bool dPoint = false){
  // There are only a few characters that make sense here :
  // '0','1','2','3','4','5','6','7','8','9','0',
  // 'A','b','c','d','E','F','H','L','P',
  lc.setChar(0, digit, c, dPoint);
}

void printByte(byte b, int digit){
  // 0x05 r, 0x1c u, B00010000 i, 0x15 n, 0x1D o, 0x0f t
  /*
   *  _6_
   * |   |
   * 1   5
   * |_0_|
   * |   |
   * 2   4
   * |_3_|
   * 
   * 7-6-5-4-3-2-1-0 = byte
   * 0 0 0 0 1 1 1 1 = 't' = 0x0F
   */
  lc.setRow(0, digit, b);
}

void idleMenuTimeout(bool reset=false){
  unsigned long currentMillisTO = millis();
  if (reset) {
    previousMillisTO = currentMillisTO;
    return;
  }

  if (displayState > DATE){
      if ((unsigned long)(currentMillisTO - previousMillisTO >= 30*interval)) {
        displayState = TIME;
        previousMillisTO = currentMillisTO;
      }
  }
}

void updateTime(){
  if (displayState <= DATE) {
    DateTime now = rtc.now();
    hourupg=now.hour();
    minupg=now.minute();
    secupg=now.second();
    dayupg=now.day();
    monthupg=now.month();
    yearupg=now.year();
  }
}

void printTime(){
  unsigned long currentMillis = millis();
  if ((unsigned long)(currentMillis - previousMillis >= interval)) {
    dotFlag = !dotFlag;
    previousMillis = currentMillis;
  }

  printNum(secupg, 1, dotFlag);
  printNum(minupg, 3, dotFlag);
  printNum(hourupg, 5, dotFlag);
}

void printDate(){
  printNum(yearupg,1);
  printNum(monthupg, 3, true);
  printNum(dayupg, 5, true);
}

void printTemp(){
  int t = rtc.getTemperature();
  printByte(0x4E, 0); // o
  printByte(0x63, 1); // C
  printNum(t, 3);
}

/////////////////////////////////////MENU/////////////////////////////////////

void menuControl(){  
  switch (btnCtrl)
  {
    case SET:
      displayState = (menuStates)((uint8_t)displayState + 1);
      btnCtrl = NONE;
//      previousMillis = 0;
      break;
    default:
      break;
  }
}

void DisplaySetHour()
{
  // Setting the hours
  switch (btnCtrl)
  {
  case UP:
    hourupg = (hourupg == 23 ? 0 : hourupg+1);
    break;
  case DOWN:
    hourupg = (hourupg == 0 ? 23 : hourupg-1);
    break;
  default:
    break;
  }
  printByte(0,0);
  printByte(0,1);
  printNum(hourupg, 3);
  printChar('h', 4, true);
  printChar('h', 5); // i.e. hh. 12
  btnCtrl = NONE;
  idleMenuTimeout();
}

void DisplaySetMinute()
{
  // Setting the minutes
  switch (btnCtrl)
  {
  case UP:
    minupg = (minupg == 59 ? 0 : minupg+1);
    break;
  case DOWN:
    minupg = (minupg == 0 ? 59 : minupg-1);
    break;
  default:
    break;
  }
  printNum(minupg, 3);
  printByte(0x20, 4); //i.e. '. 45
  printByte(0x20, 5); 
  btnCtrl = NONE;
  idleMenuTimeout();
}
  
void DisplaySetYear()
{
// setting the year
  switch (btnCtrl)
  {
  case UP:
    minupg = (yearupg == 99 ? 0 : yearupg+1);
    break;
  case DOWN:
    yearupg = (yearupg == 0 ? 0 : yearupg-1);
    break;
  default:
    break;
  }
  printNum(yearupg, 3); // i.e. YY. 20
  printByte(0x3B, 4); 
  printByte(0x3B, 5);
  btnCtrl = NONE;
  idleMenuTimeout();
}

void DisplaySetMonth()
{
// Setting the month
  switch (btnCtrl)
  {
  case UP:
    monthupg = (monthupg == 12 ? 1 : monthupg+1);
    break;
  case DOWN:
    monthupg = (monthupg == 1 ? 12 : monthupg-1);
    break;
  default:
    break;
  }
  printNum(monthupg, 3);
  printByte(0x15, 4);
  printByte(0x1D, 5);
  btnCtrl = NONE;
  idleMenuTimeout();
}

void DisplaySetDay()
{
// Setting the day
  switch (btnCtrl)
  {
  case UP:
    dayupg = (monthupg == 31 ? 1 : dayupg+1);
    break;
  case DOWN:
    dayupg = (dayupg == 1 ? 31 : dayupg-1);
    break;
  default:
    break;
  }
  printNum(dayupg, 3, false);
  printByte(0x3D, 4);
  printByte(0x3D, 5);
  btnCtrl = NONE;
  idleMenuTimeout();
}

void DisplayOptions()
{
// Setting the day
  switch (btnCtrl)
  {
  case UP:
  case DOWN:
    displayState = (menuStates)((uint8_t)displayState + 1);
    break;
  default:
    break;
  }
  printByte(0x15,0); // n
  printChar('0',1);
  printByte(0x06,2); // I
  printByte(0x0F,3); // t
  printChar('P',4);
  printChar('0',5);
  btnCtrl = NONE;
  idleMenuTimeout();
}

void saveSettings()
{
// Variable saving
  printChar('E', 2, false);
  printByte(0x15, 3); //n
  printByte(0x1D, 4); // o
  printChar('d', 5, false);
  rtc.adjust(DateTime(yearupg,monthupg,dayupg,hourupg,minupg,0));
  idleMenuTimeout(true);
  delay(1000);
  displayState=TIME;
}

/////////////////////////////////////PEREFERIALS/////////////////////////////////////
void readAllBtn(){
  for (uint8_t i = 0; i < ARRAY_SIZE(BTN); i++)
  {
    MD_UISwitch::keyResult_t state = BTN[i]->read();
    switch (state)
    {
    case MD_UISwitch::KEY_NULL:
      break;
    case MD_UISwitch::KEY_PRESS:
    case MD_UISwitch::KEY_RPTPRESS:
      btnCtrl = (buttons)i;
      idleMenuTimeout(true);
      break;
    default:
      break;
    }
  }
}

void updateDisplay(){

  switch (displayState)
  {
  case TIME:
  case DATE:
    if (secupg > 30 && secupg < 40) {
      printDate();
    }
    else {
      printTime();
    } 
    break;
  case OPTIONS:
    DisplayOptions();
    break;
  case SET_HOUR:
    DisplaySetHour();
    break;
  case SET_MIN:
    DisplaySetMinute();
    break;
  case SET_YEAR:
    DisplaySetYear();
    break;
  case SET_MONTH:
    DisplaySetMonth();
    break;
  case SET_DAY:
    DisplaySetDay();
    break;
  case SAVE:
    saveSettings();
    break;
  default:
    displayState=TIME;
    break;
  }

}

void updateOther(){
  mp3.check();        // run the mp3 receiver
  EVERY_N_MILLISECONDS( 20) {
    pacifica_loop();
    FastLED.show();
  }
}

void playerControl()
{ 
  if (displayState == TIME || displayState == DATE){
    switch (btnCtrl)
    {
    case UP:
      mp3.playNext();
      break;
    case DOWN:
      if (playerPause) mp3.playPause(); else mp3.playStart();
      break;
    default:
      break;
    }
    btnCtrl = NONE;
  }
}

/////////////////////////////////////SETUP/////////////////////////////////////
void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < ARRAY_SIZE(BTN); i++)
  {
    BTN[i] = new MD_UISwitch_Digital(SW_PIN[i], DIGITAL_SWITCH_ACTIVE);
    BTN[i]->begin();
    BTN[i]->enableRepeatResult(true);
  }
  
  Wire.begin();
  delay(3000); // wait for console opening

  if (! rtc.begin()) {
    Serial.println("No RTC found");
    while (1);
  }
  
  if (rtc.lostPower()) {
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2020, 2, 26, 21, 0, 0));
  }
  
  // wake up MAX72XX from power saving mode
  lc.shutdown(0,false);
  /* Set the brightness to a brightest value 15, darkest is 0 */
  lc.setIntensity(0,15);
  /* and clear the display */
  lc.clearDisplay(0);
  // set number of digits 6
  lc.setScanLimit(0, 5);

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection( TypicalLEDStrip );
  FastLED.setMaxPowerInVoltsAndMilliamps( 5, MAX_POWER_MILLIAMPS);

//   initialize global libraries
  mp3.begin();
  mp3.setSynchronous(true);
  mp3.playFolderRepeat(PLAY_FOLDER);
  btnCtrl = NONE;
  displayState = TIME;
  printTime();
  printDate();
}

/////////////////////////////////////LOOP/////////////////////////////////////
void loop() {
  updateTime();
  readAllBtn();
  menuControl();
  playerControl();
  updateDisplay();
  updateOther();
}


//////////////////////////////////////////////////////////////////////////
//
// The code for this animation is more complicated than other examples, and 
// while it is "ready to run", and documented in general, it is probably not 
// the best starting point for learning.  Nevertheless, it does illustrate some
// useful techniques.
//
//////////////////////////////////////////////////////////////////////////
//
// In this animation, there are four "layers" of waves of light.  
//
// Each layer moves independently, and each is scaled separately.
//
// All four wave layers are added together on top of each other, and then 
// another filter is applied that adds "whitecaps" of brightness where the 
// waves line up with each other more.  Finally, another pass is taken
// over the led array to 'deepen' (dim) the blues and greens.
//
// The speed and scale and motion each layer varies slowly within independent 
// hand-chosen ranges, which is why the code has a lot of low-speed 'beatsin8' functions
// with a lot of oddly specific numeric ranges.
//
// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https://goo.gl/maps/QQgd97jjHesHZVxQ7
//
CRGBPalette16 pacifica_palette_1 = 
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50 };
CRGBPalette16 pacifica_palette_2 = 
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F };
CRGBPalette16 pacifica_palette_3 = 
    { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33, 
      0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF };


void pacifica_loop()
{
  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011,10,13));
  sCIStart2 -= (deltams21 * beatsin88(777,8,11));
  sCIStart3 -= (deltams1 * beatsin88(501,5,7));
  sCIStart4 -= (deltams2 * beatsin88(257,4,6));

  // Clear out the LED array to a dim background blue-green
  fill_solid( leds, NUM_LEDS, CRGB( 2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary over time
  pacifica_one_layer( pacifica_palette_1, sCIStart1, beatsin16( 3, 11 * 256, 14 * 256), beatsin8( 10, 70, 130), 0-beat16( 301) );
  pacifica_one_layer( pacifica_palette_2, sCIStart2, beatsin16( 4,  6 * 256,  9 * 256), beatsin8( 17, 40,  80), beat16( 401) );
  pacifica_one_layer( pacifica_palette_3, sCIStart3, 6 * 256, beatsin8( 9, 10,38), 0-beat16(503));
  pacifica_one_layer( pacifica_palette_3, sCIStart4, 5 * 256, beatsin8( 8, 10,28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps();

  // Deepen the blues and greens a bit
  pacifica_deepen_colors();
}

// Add one layer of waves into the led array
void pacifica_one_layer( CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    waveangle += 250;
    uint16_t s16 = sin16( waveangle ) + 32768;
    uint16_t cs = scale16( s16 , wavescale_half ) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16( ci) + 32768;
    uint8_t sindex8 = scale16( sindex16, 240);
    CRGB c = ColorFromPalette( p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps()
{
  uint8_t basethreshold = beatsin8( 9, 55, 65);
  uint8_t wave = beat8( 7 );
  
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t threshold = scale8( sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();
    if( l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8( overage, overage);
      leds[i] += CRGB( overage, overage2, qadd8( overage2, overage2));
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors()
{
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].blue = scale8( leds[i].blue,  145); 
    leds[i].green= scale8( leds[i].green, 200); 
    leds[i] |= CRGB( 2, 5, 7);
  }
}
