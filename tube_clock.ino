#include <LedControl.h>
#include <RTClib.h> // concider uRTClib library https://protosupplies.com/product/ds3231-rtc-with-eeprom-module/#:~:text=The%20DS3231%20RTC%20chip%20is,the%20EEPROM%20is%20at%200x57.
#include <Wire.h>
#include <MD_UISwitch.h>
#include <MD_YX5300.h>
#include <FastLED.h>
#include <uEEPROMLib.h>

// TODO: Change Minute by digit, i.e. 23 will allow to change 2 first from 0 to 5 and 3 next from 0 to 9
// TODO: Change backlight effects by a button press
// TODO: Add Alarms with MP3 sounds
// TODO: Add "Speak Time" function
// TODO: Add digit change effects i.e. running numbers or folding/fading in and out, add random glitch effect

// Enable debug output - set to non-zero value to enable.
#define DEBUG 0

#ifdef DEBUG
#define PRINT(s,v)    { Serial.print(F(s)); Serial.print(v); }
#define PRINTX(s,v)   { Serial.print(F(s)); Serial.print(v, HEX); }
#define PRINTS(s)     { Serial.print(F(s)); }
#else
#define PRINT(s,v)
#define PRINTX(s,v)
#define PRINTS(s)
#endif

//************Pins*****************//
const uint8_t ARDUINO_RX=4;    // connect to TX of MP3 Player module
const uint8_t ARDUINO_TX=3;    // connect to RX of MP3 Player module
const uint8_t PLAY_FOLDER=1;   // tracks are all placed in this folder

const uint8_t DATA_PIN=8;
const uint8_t NUM_LEDS=3;
const uint8_t MAX_POWER_MILLIAMPS=100;
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define EEPROM_ADDR         0x57

const uint8_t SW_PIN[] = { 9, 10, 11 }; // Button SET // Button + // Button -
const uint8_t DIGITAL_SWITCH_ACTIVE = LOW;

//************EEPROM**************//
uEEPROMLib eeprom(EEPROM_ADDR);

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
enum playStatus_t { S_PAUSED, S_PLAYING, S_STOPPED };
enum playMode_t { M_SEQ, M_SHUFFLE, M_LOOP, M_EJECTED };
struct    // contains all the running status information
{
  bool initializing;      // loading data from the device
  bool waiting;           // waiting initialization response

  playMode_t playMode;    // playing mode 
  playStatus_t playStatus;// playing status
  uint16_t numTracks;     // number of tracks in this folder
  int16_t curTrack;       // current track being played
  uint16_t volume;        // the current audio volume
} S;
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
int alarm1_h;
int alarm1_m;

bool dotFlag = false;
bool hourlySpeak = true;
bool isAlarm1_active = false;
bool isAlarm1_enabled = false;

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
  SET_ALARM1, 
  SET_ALARM1_H, // set alarm1 hour
  SET_ALARM1_M, // set alarm1 minute
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

void tickTimer(){
  unsigned long currentMillis = millis();
  if ((unsigned long)(currentMillis - previousMillis >= interval)) {
    dotFlag = !dotFlag;
    previousMillis = currentMillis;
  }
}

void playSound(){
  if (hourlySpeak and minupg == 0 and secupg == 0 and hourupg >= 7 and hourupg < 21){
    mp3.playTrack(1); // add one becaue it's zero based
    // TODO: get number of tracks in th ecurrent folder and pick the number next to currtrack
  }
}

void printTime(){
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

void printAlarm1(){
  printChar('A', 5);
  printChar('1', 4, isAlarm1_enabled);
  printNum(alarm1_h, 3, dotFlag);
  printNum(alarm1_m, );
}

////////////////////////////////////////LED//////////////////////////////////////////
void setLedsAllGreen(){
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Green;
  }
  FastLED.show();
}

void setLedsAllRed(){
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
}

void setLedsAllBlack(){
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}

void blinkLed(uint16_t num=0, CRGB color=CRGB::OrangeRed){
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i==num && dotFlag){
      leds[i] = color;
      continue;
      }
    leds[i] = CRGB::Black;
  }
  FastLED.show();
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
  printTime();
  blinkLed(0);
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
  btnCtrl = NONE;
  printTime();
  blinkLed(1);
  idleMenuTimeout();
}
  
void DisplaySetYear()
{
// setting the year
  switch (btnCtrl)
  {
  case UP:
    yearupg = (yearupg == 99 ? 0 : yearupg+1);
    break;
  case DOWN:
    yearupg = (yearupg == 0 ? 99 : yearupg-1);
    break;
  default:
    break;
  }
  btnCtrl = NONE;
  printDate();
  blinkLed(2);
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
  btnCtrl = NONE;
  printDate();
  blinkLed(1);
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
  btnCtrl = NONE;
  printDate();
  blinkLed(0);
  idleMenuTimeout();
}

void DisplayOptions()
{
  setLedsAllRed();
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
  printByte(0x15,0);      // n
  printByte(0x1D, 1);     // o
  printByte(B00010000,2); // i
  printByte(0x0F,3);      // t
  printChar('P',4);       // P
  printByte(0x1D, 5);     // o
  
  btnCtrl = NONE;
  idleMenuTimeout();
}

void saveSettings()
{
  idleMenuTimeout(true);
  setLedsAllGreen();
// Variable saving
  printByte(0, 0); // 
  printByte(0, 1); // 
  printChar('E', 2, false); //E
  printByte(0x15, 3); //n
  printByte(0x1D, 4); // o
  printChar('d', 5, false); //d
  rtc.adjust(DateTime(yearupg,monthupg,dayupg,hourupg,minupg,0));
  saveAlarm1NVRam();
  delay(1000);
  displayState=TIME;
}

void displaySetAlarm1()
{
// Enabling the Alarm1
  switch (btnCtrl)
  {
  case UP:
    isAlarm1_enabled = true;
    break;
  case DOWN:
    isAlarm1_enabled = false;
    break;
  default:
    break;
  }
  btnCtrl = NONE;
  printAlarm1();
  if(isAlarm1_enabled)
    blinkLed(0, CRGB::Green);
  else
    blinkLed(0, CRGB::Red);
  idleMenuTimeout();  
}

void displaySetAlarm1_H()
{
// Setting the Alarm1 hours
  switch (btnCtrl)
  {
  case UP:
    alarm1_h = (alarm1_h == 23 ? 0 : alarm1_h+1);
    break;
  case DOWN:
    alarm1_h = (alarm1_h == 0 ? 23 : alarm1_h-1);
    break;
  default:
    break;
  }
  btnCtrl = NONE;
  printAlarm1();
  blinkLed(1, CRGB::Red);
  idleMenuTimeout();  
}

void displaySetAlarm1_M()
{
// Setting the Alarm1 minutes
  switch (btnCtrl)
  {
  case UP:
    alarm1_m = (alarm1_m == 59 ? 0 : alarm1_m+1);
    break;
  case DOWN:
    alarm1_m = (alarm1_m == 0 ? 59 : alarm1_m-1);
    break;
  default:
    break;
  }
  btnCtrl = NONE;
  printAlarm1();
  blinkLed(2, CRGB::Red);
  idleMenuTimeout();
}

/////////////////////////////////////PEREFERIALS/////////////////////////////////////


////////////////////////////////////BUTTONS//////////////////////////////////////////
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
      stopAlarm1(); // exit alarm mode on any button press
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
  case SET_ALARM1:
    displaySetAlarm1();
    break;
  case SET_ALARM1_H:
    displaySetAlarm1_H();
    break;
  case SET_ALARM1_M:
    displaySetAlarm1_M();
    break;
  case SAVE:
    saveSettings();
    break;
  default:
    displayState=TIME;
    break;
  }

}

void updateLeds(){
  if (displayState == TIME || displayState == DATE){
    if (upgmin == alarm1_m and upghour == alarm1_h and isAlarm1_enabled){
      if (dotFlag){
        setLedsAllRed();
      }
      else {
        setLedsAllBlack();
      }
      return;
    }
    EVERY_N_MILLISECONDS( 20) {
      pacifica_loop();
      FastLED.show();
    }
  }
}
/////////////////////////////////MP3/////////////////////////////////////////
bool initData(bool reset = false)
// Initialize data from the MP3 device. 
// This needs to be metered out as the data requests will generate 
// unsolicited messages to be handled in the callback - message 
// sequence must be maintained with the synchronous message processing
// stream.
// Returns true if the initialization must keep going, false when completed.
{
  static uint8_t state = 0;
  bool b = true;

  if (reset)    // just rest the sequencing
  {
    state = 0;
  }
  else
  {
    switch (state)
    {
    case 0:   // set a default state in the device and then ask for first data
      mp3.playSpecific(PLAY_FOLDER, 1);
      mp3.playPause();
      S.playMode = M_SEQ;
      S.playStatus = S_PAUSED;

      if (S.volume == 0)
        //S.volume = (mp3.volumeMax() / 3) * 2;   // 2/3 of volume to start with
        S.volume = mp3.volumeMax(); // set max volume
      mp3.volume(S.volume);

      // load number of files in the folder - needs wait for response
      mp3.queryFolderFiles(PLAY_FOLDER);
      S.waiting = true;
      state++;
      break;

    case 1: // now load the track playing - needs wait for response
      mp3.queryFile();
      S.waiting = true;
      state++;
      break;

    default:
      // end of sequence handler - reset to start
      state = 0;
      b = false;
      break;
    }
  }

  return(b);
}

void selectNextSong(int direction = 0)
// Pick the next song to play based on playing mode set.
// If direction  < 0 then select a 'previous' song, 
// otherwise the 'next' song is selected.
{
  switch (S.playMode)
  {
  case M_SHUFFLE:   // random selection
    {
      uint16_t x = random(S.numTracks) + 1;
      mp3.playTrack(x);
      PRINT("\nPlay SHUFFLE ", x);
    }
    break;

  case M_LOOP:      // replay the same track
      mp3.playTrack(S.curTrack);
      PRINTS("\nPlay LOOP");
      break;

  case M_SEQ:       // play sequential - next/previous
      if (direction < 0) 
        mp3.playPrev(); 
      else  
        mp3.playNext();
      PRINTS("\nPlay SEQ");
      break;
  }
  mp3.queryFile();    // force index the update in callback
}

void cbResponse(const MD_YX5300::cbData *status)
// Callback function used to process device unsolicited messages
// or responses to data requests
{
  PRINTS("\n");
  switch (status->code)
  {
  case MD_YX5300::STS_FILE_END:   // track has ended
    PRINTS("STS_FILE_END");
    selectNextSong();
    break;

  case MD_YX5300::STS_TF_INSERT:  // card has been inserted
    PRINTS("STS_TF_INSERT"); 
    S.initializing = initData(true);
    break;

  case MD_YX5300::STS_TF_REMOVE:  // card has been removed
    PRINTS("STS_TF_REMOVE"); 
    S.playMode = M_EJECTED;
    S.playStatus = S_STOPPED;
    break;

  case MD_YX5300::STS_PLAYING:   // current track index 
    PRINTS("STS_PLAYING");    
    S.curTrack = status->data;
    break;

  case MD_YX5300::STS_FLDR_FILES: // number of files in the folder
    PRINTS("STS_FLDR_FILES"); 
    S.numTracks = status->data;
    break;

  // unhandled cases - used for debug only
  case MD_YX5300::STS_VOLUME:     PRINTS("STS_VOLUME");     break;
  case MD_YX5300::STS_TOT_FILES:  PRINTS("STS_TOT_FILES");  break;
  case MD_YX5300::STS_ERR_FILE:   PRINTS("STS_ERR_FILE");   break;
  case MD_YX5300::STS_ACK_OK:     PRINTS("STS_ACK_OK");     break;
  case MD_YX5300::STS_INIT:       PRINTS("STS_INIT");       break;
  case MD_YX5300::STS_STATUS:     PRINTS("STS_STATUS");     break;
  case MD_YX5300::STS_EQUALIZER:  PRINTS("STS_EQUALIZER");  break;
  case MD_YX5300::STS_TOT_FLDR:   PRINTS("STS_TOT_FLDR");   break;
  default: PRINTX("STS_??? 0x", status->code); break;
  }

  PRINTX(", 0x", status->data);
  S.waiting = false;
}

void processPlayMode(void)
// Read the mode selection switch and act if it has been pressed
//  - Start/pause track playback with simple press
//  - Random/repeat/single playback cycle with long press
{
  switch (btnCtrl)
  {
  case UP:
    if(mp3.playNext()) S.playStatus = S_PLAYING;
    break;
  case DOWN:  
    switch (S.playStatus)
      {
      case S_PLAYING:
        if (mp3.playPause()) S.playStatus = S_PAUSED;
      break;

      case S_PAUSED:
        if (mp3.playStart()) S.playStatus = S_PLAYING;
      break;
      }
    break;
  default:
    break;
  }
  playSound();
  btnCtrl = NONE;
 
// switch (k)
//  {
//
//  case MD_UISwitch::KEY_LONGPRESS: // random/loop/single cycle
//    switch (S.playMode)
//    {
//    case M_SEQ:     S.playMode = M_LOOP;    PRINTS("\nMode LOOP");    break;
//    case M_LOOP:    S.playMode = M_SHUFFLE; PRINTS("\nMode SHUFFLE"); break;
//    case M_SHUFFLE: S.playMode = M_SEQ;     PRINTS("\nMode SINGLE");  break;
//    }
//    break;
//  }
}

void playerControl()
{ 
  if (displayState == TIME || displayState == DATE){
    mp3.check();        // run the mp3 receiver
    // Initialization must preserve the unsolicited queue order so it 
    // stops any normal synchronous calls from happening
    if (S.initializing && !S.waiting)
        S.initializing = initData();
    else {
        processPlayMode();  // set the current play mode (switch selection)
    }
  }
}

void checkAlarm1()
{
  if (!isAlarm1_enabled){
    return;
  }
    if (minupg == alarm1_m and hourupg == alarm1_h){
      if (isAlarm1_active)
        return;
      else {
        isAlarm1_active = true;
        selectNextSong(); // add one becaue it's zero based
      }
      // TODO: get number of tracks in th ecurrent folder and pick the number next to currtrack
    } 
    else {
      stopAlarm1();
    }  
}

void stopAlarm1(){
  if(isAlarm1_active){
    isAlarm1_active = false;
    mp3.playPause();
    btnCtrl = NONE;
  }       
}

void readAlarm1NVRam(){
  eeprom.eeprom_read(0, &alarm1_h);
  
  if (alarm1_h < 0 or alarm1_h > 23){
    alarm1_h = 12;
  }

  eeprom.eeprom_read(1, &alarm1_m);

  if (alarm1_m <0 or alarm1_m > 59){
    alarm1_m = 0;
  }
  uint8_t tmp_b;
  eeprom.eeprom_read(2, &tmp_b);
  isAlarm1_enabled = (bool)tmp_b;
}

void saveAlarm1NVRam(){
  if (alarm1_h >=0 and alarm1_h <= 23){
    if (!eeprom.eeprom_write(0, alarm1_h)) {
      Serial.println("Failed to store Alarm H");
    } else {
      Serial.println("Alarm H correctly stored");
    }
  }
  
  if (alarm1_m >=0 and alarm1_m <= 59){
    if (!eeprom.eeprom_write(1, alarm1_m)) {
      Serial.println("Failed to store Alarm M");
    } else {
      Serial.println("Alarm M correctly stored");
    }
  }
  
  if (!eeprom.eeprom_write(2, (uint8_t)isAlarm1_enabled)) {
    Serial.println("Failed to store Alarm Enabled");
  } else {
    Serial.println("Alarm Enabled correctly stored");
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

  readAlarm1NVRam();
  
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

  // initialize global libraries
  mp3.begin();
  mp3.setSynchronous(true);
  mp3.setCallback(cbResponse);
  S.initializing = initData(true);
  
  btnCtrl = NONE;
  displayState = TIME;
  printTime();
  printDate();
  
}

/////////////////////////////////////LOOP/////////////////////////////////////
void loop() {
  tickTimer();
  updateTime();
  checkAlarm1();
  readAllBtn();
  menuControl();
  playerControl();
  updateDisplay();
  updateLeds();
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
