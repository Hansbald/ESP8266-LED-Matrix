#include "ESP8266WiFi.h"
#include <WiFiUDP.h>
#include <SPI.h>
#include <SD.h>
#include <FastLED.h>
#include <WiFiManager.h>
#include <FastLED.h>
#include <Button.h>

//UDP Variables
#define UDP_TX_PACKET_MAX_SIZE 950 //increase UDP size
unsigned int localPort = 80;  // local port to listen on
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];// buffer to hold incoming packet
char ReplyBuffer[4] = "ACK";      // a string to send back
char nameBuffer[11] = "123456789";
WiFiUDP Udp;

//FastLED Defines
#define LED_DATA_PIN  3
#define COLOR_ORDER GRB
#define CHIPSET WS2812B
#define BRIGHTNESS 64
const uint8_t kMatrixWidth = 10;
const uint8_t kMatrixHeight = 10;
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
const bool kMatrixSerpentineLayout = true;

//FastLED Variables
CRGB leds[kMatrixWidth * kMatrixHeight];
char charBuffer[3];
uint8_t R;
uint8_t G;
uint8_t B;
uint8_t fileAmount;
uint32_t timer;
uint32_t lastTimer;
#define DELAY_TIME 5000
#define SPECTRUM_ANALYZER_INTERRUPT_PIN 16
bool modeRandom = false;
uint8_t frameCounter = 1; //Ignore SYSTEM file
File root;

//Noise/Animation Variables
#define HOLD_PALETTES_X_TIMES_AS_LONG 1 // 1 = 5 sec per palette; 2 = 10 sec per palette
static uint16_t x;
static uint16_t y;
static uint16_t z;
uint16_t _speed;
uint16_t _scale;
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];
CRGBPalette16 currentPalette( PartyColors_p );
uint8_t       colorLoop = 1;

//Button Variables
#define FILE_RANDOM_MODE 2
Button fileMode(FILE_RANDOM_MODE, true, true, 20);
#define MODE_SELECT_PIN 5
Button modeSelect(MODE_SELECT_PIN, true, true, 20);
bool _spectrumEnabled = false;


//Matrix Functions
uint16_t XY( uint8_t x, uint8_t y){
  uint16_t i;

  if( kMatrixSerpentineLayout == false) {
    i = (y * kMatrixWidth) + x;
  }

  if( kMatrixSerpentineLayout == true) {
    if( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (kMatrixWidth - 1) - x;
      i = (y * kMatrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * kMatrixWidth) + x;
    }
  }

  return i;
}

void fillnoise8() {
	// Fill the x/y array of 8-bit noise values using the inoise8 function.
	//uint16_t speed = 20; <Default values
	//uint16_t scale = 311;
	for(int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = _scale * i;
    for(int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = _scale * j;
      noise[i][j] = inoise8(x + ioffset,y + joffset,z);
    }
  }
  z += _speed;
}

void SetupRandomPalette(){
  currentPalette = CRGBPalette16(
                      CHSV( random8(), 255, 32),
                      CHSV( random8(), 255, 255),
                      CHSV( random8(), 128, 255),
                      CHSV( random8(), 255, 255));
}

void SetupBlackAndWhiteStripedPalette(){
  // 'black out' all 16 palette entries...
  fill_solid( currentPalette, 16, CRGB::Black);
  // and set every fourth one to white.
  currentPalette[0] = CRGB::White;
  currentPalette[4] = CRGB::White;
  currentPalette[8] = CRGB::White;
  currentPalette[12] = CRGB::White;
}

void SetupPurpleAndGreenPalette(){
  CRGB purple = CHSV( HUE_PURPLE, 255, 255);
  CRGB green  = CHSV( HUE_GREEN, 255, 255);
  CRGB black  = CRGB::Black;

  currentPalette = CRGBPalette16(
    green,  green,  black,  black,
    purple, purple, black,  black,
    green,  green,  black,  black,
    purple, purple, black,  black );
}

void ChangePaletteAndSettingsPeriodically(){
  uint8_t secondHand = ((millis() / 1000) / HOLD_PALETTES_X_TIMES_AS_LONG) % 60;
  static uint8_t lastSecond = 99;

  if( lastSecond != secondHand) {
    lastSecond = secondHand;
    if( secondHand ==  0)  { currentPalette = RainbowColors_p;         _speed = 20; _scale = 30; colorLoop = 1; }
    if( secondHand ==  5)  { SetupPurpleAndGreenPalette();             _speed = 10; _scale = 50; colorLoop = 1; }
    if( secondHand == 10)  { SetupBlackAndWhiteStripedPalette();       _speed = 20; _scale = 30; colorLoop = 1; }
    if( secondHand == 15)  { currentPalette = ForestColors_p;          _speed =  8; _scale =120; colorLoop = 0; }
    if( secondHand == 20)  { currentPalette = CloudColors_p;           _speed =  4; _scale = 30; colorLoop = 0; }
    if( secondHand == 25)  { currentPalette = LavaColors_p;            _speed =  8; _scale = 50; colorLoop = 0; }
    if( secondHand == 30)  { currentPalette = OceanColors_p;           _speed = 20; _scale = 90; colorLoop = 0; }
    if( secondHand == 35)  { currentPalette = PartyColors_p;           _speed = 20; _scale = 30; colorLoop = 1; }
    if( secondHand == 40)  { SetupRandomPalette();                     _speed = 20; _scale = 20; colorLoop = 1; }
    if( secondHand == 45)  { SetupRandomPalette();                     _speed = 50; _scale = 50; colorLoop = 1; }
    if( secondHand == 50)  { SetupRandomPalette();                     _speed = 90; _scale = 90; colorLoop = 1; }
    if( secondHand == 55)  { currentPalette = RainbowStripeColors_p;   _speed = 30; _scale = 20; colorLoop = 1; }
  }
}

void displaySimpleNoise(uint16_t speed, uint16_t scale){
	static uint8_t ihue=0;
	_speed = speed;
	_scale = scale;
	fillnoise8();
	for(int i = 0; i < kMatrixWidth; i++) {
		for(int j = 0; j < kMatrixHeight; j++) {
			leds[XY(i,j)] = CHSV(noise[j][i],255,noise[i][j]);
			// leds[XY(i,j)] = CHSV(ihue + (noise[j][i]>>2),255,noise[i][j]);
		}
	}
	ihue+=1;
	FastLED.show();
}

void displayColorPaletteNoise(uint16_t speed, uint16_t scale){
	static uint8_t ihue=0;
	_speed = speed;
	_scale = scale;
	fillnoise8();

  for(int i = 0; i < kMatrixWidth; i++) {
    for(int j = 0; j < kMatrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.
      uint8_t index = noise[j][i];
      uint8_t bri =   noise[i][j];
      // if this palette is a 'loop', add a slowly-changing base value
      if( colorLoop) {
        index += ihue;
      }

      // brighten up, as the color palette itself often contains the
      // light/dark dynamic range desired
      if( bri > 127 ) {
        bri = 255;
      } else {
        bri = dim8_raw( bri * 2);
      }
      CRGB color = ColorFromPalette( currentPalette, index, bri);
      leds[XY(i,j)] = color;
    }
  }
  ihue+=1;
	FastLED.show();
}

//SD/Frame Functions
uint8_t getFileAmount(){
  root = SD.open("/");
  uint8_t i = 0; //counter

  while (true) {
    File entry =  root.openNextFile();
    if (! entry) {
      // no more files
      root.close();
      return i;
      break;
    }
    i++;
    entry.close();
  }
}

bool readRGBFile(File dataFile){
  if (dataFile){
    Serial.print("[readRGBFile] Starting Read of File: ");
    Serial.println(dataFile.name());
    for(uint8_t i = 0; i < 100; i++){ //Run 100 Times for all Pixels
      delay(1); //Kick the Watchdog Awake
      //Serial.print("Starting Read No.: ");
      //Serial.println(i);

      uint8_t k = 0;

      for(k = 0; k < 3; k++){ //Read First 3 Chars (Red)
        char c = dataFile.read();
        charBuffer[k] = c;
      }
      R = atoi(charBuffer);
      //Serial.print("R: ");
      //Serial.println(R);

      for(k = 0; k < 3; k++){ //Read Next 3 Chars (Green)
        char c = dataFile.read();
        charBuffer[k] = c;
      }
      G = atoi(charBuffer);
      //Serial.print("G: ");
      //Serial.println(G);

      for(k = 0; k < 3; k++){ //Read Next 3 Chars (Blue)
        char c = dataFile.read();
        charBuffer[k] = c;
      }
      B = atoi(charBuffer);
      //Serial.print("B: ");
      //Serial.println(B);

      leds[i] = CRGB(R, G, B); //Add Colors to LED
    }
  }else{
    Serial.println("[readRGBFile] Failed to OpenFile...");
    return false;
  }
  dataFile.close();
	Serial.println("[readRGBFile] Succesfully set leds[]!");
  return true;
}

void playRandomFrame(){
  root = SD.open("/");
  uint8_t ranNum = random8(1, fileAmount); //Uses random from FastLED libary
  //uint8_t ranNum = ESP8266TrueRandom.random(1,fileAmount);
  uint8_t i = 0; //counter
  Serial.println(ranNum);
  while (true) {
    File entry =  root.openNextFile();
    if (!entry) {
      // no more files
      root.close();
      break;
    }

    if(i == ranNum){
      root.close();
			Serial.print("[RANDOM] Name of File: ");
			Serial.println(entry.name());
      readRGBFile(entry);
			break;
    }
    i++;
    entry.close();
  }
}

void playNextFrame(){
	root = SD.open("/");
	if(frameCounter == fileAmount){frameCounter = 1; }
	uint8_t i = 0; //local counter
	while (true) {
		File entry =  root.openNextFile();
		if (!entry) {
			// no more files
			root.close();
			break;
		}

		if(i == frameCounter){
			root.close();
			Serial.print("[NEXT] Name of File: ");
			Serial.println(entry.name());
			readRGBFile(entry);
			break;
		}
		i++;
		entry.close();
	}
	frameCounter++;
}

bool writeToSD(char* filename, char* payload){
  File target = SD.open(filename, FILE_WRITE);
  // if the file opened okay, write to it:
  if (target) {
    Serial.print("[SD-Write] Writing to ");
    Serial.print(filename);
    Serial.println("...");

    target.println(payload);
    // close the file:
    target.close();
    fileAmount = getFileAmount();
    Serial.println("[SD-Write] done.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("[SD-Write] error opening file");
  }
}

//Networking Functions
bool checkForPacket(){
  int payload = Udp.parsePacket();
  if (payload) {
    Serial.print(millis() / 1000);
    Serial.print("[PacketCheck] :Packet of ");
    Serial.print(payload);
    Serial.print(" Bytes received from ");
    Serial.print(Udp.remoteIP());
    Serial.print(":");
    Serial.println(Udp.remotePort());
    // We've received a packet, read the data from it
    //But first clean the buffer array
    memset(packetBuffer, 0, sizeof(packetBuffer));
    Udp.read(packetBuffer, payload); // read the packet into the buffer
    // display the packet contents in HEX
    if (strstr(packetBuffer, "NAME=")){
      Serial.println("[PacketCheck] Entering NAME= section...");
      for(int i = 0; i < (sizeof(nameBuffer) - 5); i++){
        nameBuffer[i] = packetBuffer[i + 5];
      }
      nameBuffer[sizeof(nameBuffer)-5] = '.';
      nameBuffer[sizeof(nameBuffer)-4] = 'T';
      nameBuffer[sizeof(nameBuffer)-3] = 'X';
      nameBuffer[sizeof(nameBuffer)-2] = 'T';
      Serial.print("[PacketCheck] Name: ");
      Serial.println(nameBuffer);
      return false;
    }
    for (int i=1;i<=payload;i++){
      delay(1); //Kick the Watchdog Awake
      Serial.print(packetBuffer[i-1], HEX);
      if (i % 32 == 0){
        Serial.println();
      }
      else Serial.print(' ');
    } // end for

    Serial.println();
    Serial.println();
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort()); //Send answer to Packet-Sender
    Udp.write(ReplyBuffer);
    Udp.endPacket();
    return true;
  } // end if
  return false;
}


void setup(void){
  ESP.wdtEnable(10000);
  Serial.begin(9600);
	digitalWrite(SPECTRUM_ANALYZER_INTERRUPT_PIN, LOW); //Make sure Spectrum Analyzer is off
	Serial.println("[Spectrum] Disabled Spectrum Analyzer");
  Serial.print("[SD-Init] Initializing SD card...");
  if (!SD.begin()) {
    Serial.println("initialization failed!");
    ESP.restart();
    return;
  }
  Serial.println("initialization done.");

  fileAmount = getFileAmount();
  Serial.print("[SD-Init] Amount of Files: ");
  Serial.println(fileAmount);
  //WifiManager
  WiFiManager wifiManager;
	//Set IP
	IPAddress _ip = IPAddress(192, 168, 192, 222);
  IPAddress _gw = IPAddress(192, 168, 192, 1);
  IPAddress _sn = IPAddress(255, 255, 255, 0);
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

	//Try to connect to saved AP or start Fallback Portal
  wifiManager.autoConnect("LED-Matrix-by-Hansbald");

  Serial.print("[UDP-Init] Starting UDP at Port: ");
  Serial.println(localPort);
  Udp.begin(localPort);

	//Initilaize FastLED
	Serial.print("[FastLED] Adding LEDs on Pin ");
	Serial.print(LED_DATA_PIN);
	Serial.print(" with color order");
	Serial.println(COLOR_ORDER);
	FastLED.addLeds<CHIPSET, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness( BRIGHTNESS );
	// Initialize noise coordinates to some random values
	Serial.println("[Noise] Initializing Noise coordinates with random values...");
	x = random16();
  y = random16();
  z = random16();

  Serial.println("[Setup done...]");
}

void loop() {
	timer = millis();
	ChangePaletteAndSettingsPeriodically();
  if(checkForPacket()){
    writeToSD(nameBuffer, packetBuffer);
  }

	fileMode.read();
	modeSelect.read();

	if(modeSelect.wasPressed()){
			_spectrumEnabled = !_spectrumEnabled;
			if(_spectrumEnabled){
				digitalWrite(SPECTRUM_ANALYZER_INTERRUPT_PIN, HIGH);
				Serial.println("[Spectrum Button] Enabling Spectrum Analyzer... ");
			}else{
				digitalWrite(SPECTRUM_ANALYZER_INTERRUPT_PIN, LOW);
				Serial.println("[Spectrum Button] Disabling Spectrum Analyzer... ");
			}
			delay(500);
	}

	if(fileMode.pressedFor(1000)){
		modeRandom = true;
	}

	if(fileMode.wasPressed()){
		modeRandom = false;
		playNextFrame();
	}

	if(modeRandom && timer - DELAY_TIME > lastTimer ){
		lastTimer = millis();
		playRandomFrame();
	}
}
