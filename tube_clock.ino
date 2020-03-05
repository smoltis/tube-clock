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
const uint8_t SW_PIN[] = { 9, 10, 11 }; // Button SET MENU' // Button + // Button -
bool playerPause = true;  // true if player is currently paused

MD_YX5300 mp3(ARDUINO_RX, ARDUINO_TX);
MD_UISwitch_Digital *BTN[ARRAY_SIZE(SW_PIN)];
RTC_DS3231 rtc;

#define FASTLED_ALLOW_INTERRUPTS 0
FASTLED_USING_NAMESPACE

#define DATA_PIN            8
#define NUM_LEDS            3
#define MAX_POWER_MILLIAMPS 100
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB

CRGB leds[NUM_LEDS];


//************Variables**************//
int hourupg;
int minupg;
int secupg;
int yearupg;
int monthupg;
int dayupg;
int menu=0;


LedControl lc=LedControl(7,5,6,1);



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

void printTime(){
  DateTime now = rtc.now();
  hourupg=now.hour();
  minupg=now.minute();
  secupg=now.second();
  
  printNum(hourupg,5);
  printNum(minupg,3);
  printNum(secupg,1);
}

void printDate(){
  DateTime now = rtc.now();
  dayupg=now.day();
  monthupg=now.month();
  yearupg=now.year();
  printNum(dayupg, 5, true);
  printNum(monthupg, 3, true);
  printNum(yearupg,1);
}

void printTemp(){
  lc.clearDisplay(0);
  int t = rtc.getTemperature();
  printNum(t, 3);
}


/////////////////////////////////////

void checkButtons(const char* buttonName, MD_UISwitch::keyResult_t state)
{ 


  char btn = 'X';
  
  if (state == MD_UISwitch::KEY_PRESS)
    btn = buttonName[1];

  if (state == MD_UISwitch::KEY_PRESS && menu == 0){
    if (btn == '2'){
      Serial.println("mp3 start");
      if (playerPause) mp3.playPause(); else mp3.playStart();
      return;
    }
    if (btn == '1'){
      mp3.playNext();
      return;
    }
  }
  
  if (btn != 'X'){
    Serial.print(buttonName);
    Serial.println(menu); 
    if(btn == '0')
      menu = menu + 1;
  }
     
  switch (menu){
      case 1:
        DisplaySetHour(btn);
        break;
      case 2:
        DisplaySetMinute(btn);
        break;
      case 3:
        DisplaySetYear(btn);
        break;
      case 4:
        DisplaySetMonth(btn);
        break;
      case 5:
        DisplaySetDay(btn);
        break;
      case 6:
        StoreAgg(); 
        delay(500);
        menu=0;
        break;
      default:
        printTime();
        break;
      }
}

void DisplaySetHour(char buttonName)
{
// time setting
  lc.clearDisplay(0);
  DateTime now = rtc.now();
  if(buttonName == '1')
  {
    if(hourupg==23)
    {
      hourupg=0;
    }
    else
    {
      hourupg=hourupg+1;
    }
  }
   if(buttonName == '2')
  {
    if(hourupg==0)
    {
      hourupg=23;
    }
    else
    {
      hourupg=hourupg-1;
    }
  }
  printByte(0x20, 4);
  printNum(hourupg, 3, false);
//  delay(200);
}

void DisplaySetMinute(char buttonName)
{
// Setting the minutes
  lc.clearDisplay(0);
  if(buttonName == '1')
  {
    if (minupg==59)
    {
      minupg=0;
    }
    else
    {
      minupg=minupg+1;
    }
  }
   if(buttonName == '2')
  {
    if (minupg==0)
    {
      minupg=59;
    }
    else
    {
      minupg=minupg-1;
    }
  }
  printByte(0x20, 5);
  printByte(0x20, 4);
  printNum(minupg, 3, false);
//  delay(200);
}
  
void DisplaySetYear(char buttonName)
{
// setting the year
  lc.clearDisplay(0);
  if(buttonName == '1')
  {    
    yearupg=yearupg+1;
  }
   if(buttonName == '2')
  {
    yearupg=yearupg-1;
  }
  printByte(0x3B, 5);
  printByte(0x3B, 4);
  printNum(yearupg, 3, false);
//  delay(200);
}

void DisplaySetMonth(char buttonName)
{
// Setting the month
  lc.clearDisplay(0);
  if(buttonName == '1')
  {
    if (monthupg==12)
    {
      monthupg=1;
    }
    else
    {
      monthupg=monthupg+1;
    }
  }
   if(buttonName == '2')
  {
    if (monthupg==1)
    {
      monthupg=12;
    }
    else
    {
      monthupg=monthupg-1;
    }
  }
  printByte(0x0D, 5);
  printByte(0x0D, 4);
  printNum(monthupg, 3, false);
//  delay(200);
}

void DisplaySetDay(char buttonName)
{
// Setting the day
  lc.clearDisplay(0);
  if(buttonName == '1')
  {
    if (dayupg==31)
    {
      dayupg=1;
    }
    else
    {
      dayupg=dayupg+1;
    }
  }
   if(buttonName == '2')
  {
    if (dayupg==1)
    {
      dayupg=31;
    }
    else
    {
      dayupg=dayupg-1;
    }
  }
  printByte(0x3D, 5);
  printByte(0x3D, 4);
  printNum(dayupg, 3, false);
//  delay(200);
}

void StoreAgg()
{
// Variable saving
  lc.clearDisplay(0);
  printChar('d', 5, false);
  printByte(0x1D, 4);
  printByte(0x15, 3);
  printChar('E', 2, false);
  rtc.adjust(DateTime(yearupg,monthupg,dayupg,hourupg,minupg,0));
//  delay(200);
}

/////////////////////////////////////
void setup() {
  Serial.begin(9600);

  for (uint8_t i = 0; i < ARRAY_SIZE(BTN); i++)
  {
    BTN[i] = new MD_UISwitch_Digital(SW_PIN[i], LOW);
    BTN[i]->begin();
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
     rtc.adjust(DateTime(2020, 2, 26, 21, 0, 0));
  }
  
  // wake up MAX72XX from power saving mode
  lc.shutdown(0,false);
  /* Set the brightness to a medium values */
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
}

void loop() {
  mp3.check();        // run the mp3 receiver
  for (uint8_t i = 0; i < ARRAY_SIZE(BTN); i++)
  {
    char name[] = "B ";

    name[1] = i + '0';  // turn into a digit
    
    checkButtons(name, BTN[i]->read());

  }
//  printTime();
  // put your main code here, to run repeatedly:
  EVERY_N_MILLISECONDS( 20) {
    pacifica_loop();
    FastLED.show();
  }
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
