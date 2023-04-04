#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h> // https://github.com/DFRobot/DFRobotDFPlayerMini

// Wifi network station credentials
//#define WIFI_SSID "DankoKenetic"
//#define WIFI_PASSWORD "qwer1234"

#define WIFI_SSID "Kotiki"
#define WIFI_PASSWORD "89214241869"

// Time offset
#define GMT_OFFSET 60 * 60 * 3  // GMT+3

// mDNS name
#define WEB_PAGE_NAME "uboat"

// Start volume
#define VOLUME_AT_START 5

// Equalizer settings. Could be: DFPLAYER_EQ_NORMAL, DFPLAYER_EQ_POP, DFPLAYER_EQ_ROCK, DFPLAYER_EQ_JAZZ, DFPLAYER_EQ_CLASSIC, DFPLAYER_EQ_BASS
#define EQ_PROFILE DFPLAYER_EQ_NORMAL

// Silence between tracks
#define TRACKS_DELAY 4000

#define TRACK_FINISED_STATE 512 // internal DFPlayer logic

// Folder positions
#define MUSIC_FOLDER  1
#define SOUNDS_FOLDER 2

// PINS
#define WHITE_LED     D4
#define RED_LED       D2
#define BLUE_LED      D3
#define EXTERNAL_LED  D1
#define PLAYER_RX     D7
#define PLAYER_TX     D8

// Local vars
ESP8266WebServer server(80);
DFRobotDFPlayerMini myDFPlayer;
SoftwareSerial mySoftwareSerial(PLAYER_RX, PLAYER_TX); // RX, TX

// State switches
bool whiteEnabled = false;
bool redEnabled = false;
bool blueEnabled = false;
bool externalEnabled = false;
bool isPlaying = false;
bool isJustStarted = true;
bool isPlayingMusic = true;
uint8_t lastMusicFile = 1;
uint8_t lastSoundFile = 1;

// Logging (could be simply redirected to TG for example)
void logToChat(String const& message) {
  Serial.println(message);
}

// Wifi and time misc
void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting...");
  }
}

String getCurrentDateTime() {
  time_t now;
  tm* timeinfo;
  time(&now);
  timeinfo = localtime(&now);
  char buf[20];
  snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
           timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  return String(buf);
}

String getCurrentTime() {
  time_t now;
  tm* timeinfo;
  time(&now);
  timeinfo = localtime(&now);
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  return String(buf);
}

void setupTime() {
  configTime(GMT_OFFSET, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(100);
    now = time(nullptr);
  }
  logToChat(F("Time synchronized: ") + getCurrentTime());
}

// DFPlayer board
void initDfPlayer() {
	logToChat(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

	if (myDFPlayer.begin(mySoftwareSerial)) {
    logToChat(F("DFPlayer Mini online."));
    myDFPlayer.EQ(EQ_PROFILE);
  }
	else
		logToChat(F("Failed to init player"));
}

// LEDS misc
void initSystem(){
    digitalWrite(WHITE_LED, HIGH);
    delay(1000);
    digitalWrite(WHITE_LED, LOW); 

    digitalWrite(RED_LED, HIGH);
    delay(1000);
    digitalWrite(RED_LED, LOW); 

    digitalWrite(BLUE_LED, HIGH);
    delay(1000);
    digitalWrite(BLUE_LED, LOW); 

    digitalWrite(EXTERNAL_LED, HIGH);
    delay(1000);
    digitalWrite(EXTERNAL_LED, LOW); 

    delay(2000);
  }

// Dummies
void onConnect();
void onWhite();
void onRed();
void onBlue();
void onExternal();
void onPlayPause();
void onNextTrack();
void onPrevTrack();
void onVolumeUp();
void onVolumeDown();
void onPlaySounds();
void onPlayMusic();
void onNotFound();

void printDetail();
void playSomething();

//----------------------------------------------------------------------
// Constructor & main loop
//----------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();

  mySoftwareSerial.begin(9600);

  pinMode(WHITE_LED, OUTPUT); 
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(EXTERNAL_LED, OUTPUT); 

  initSystem();
  
  digitalWrite(WHITE_LED, whiteEnabled); 
  digitalWrite(RED_LED, redEnabled); 
  digitalWrite(BLUE_LED, blueEnabled); 
  digitalWrite(EXTERNAL_LED, externalEnabled);

  Serial.println(F("Starting wifi client"));
  setupWiFi();
  setupTime();

  if (MDNS.begin(WEB_PAGE_NAME)) { logToChat(F("MDNS responder started")); }
    
  delay(100);
  
  server.on("/", onConnect);
  server.on("/w",  onWhite);
  server.on("/r",  onRed);
  server.on("/b",  onBlue);
  server.on("/e",  onExternal);
  server.on("/playPause",  onPlayPause);
  server.on("/nextTrack", onNextTrack);
  server.on("/prevTrack", onPrevTrack);
  server.on("/volumeUp", onVolumeUp);
  server.on("/volumeDown", onVolumeDown);
  server.on("/changeFolder", onChangeFolder);
  
  server.onNotFound(onNotFound);
  
  server.begin();
  logToChat(F("HTTP server started"));

  initDfPlayer();
  myDFPlayer.volume(VOLUME_AT_START);
}

void loop() {
  static unsigned long timer = millis();

  server.handleClient();
  MDNS.update();

  digitalWrite(WHITE_LED, whiteEnabled); 
  digitalWrite(RED_LED, redEnabled); 
  digitalWrite(BLUE_LED, blueEnabled); 
  digitalWrite(EXTERNAL_LED, externalEnabled);

  if (myDFPlayer.available())
    printDetail(myDFPlayer.readType(), myDFPlayer.read());
  
  if (millis() - timer > TRACKS_DELAY) {
    timer = millis();
    if (isPlaying && myDFPlayer.readState() == TRACK_FINISED_STATE) {
      logToChat(F("Player -> next"));
      isPlayingMusic ? ++lastMusicFile : ++lastSoundFile;
      //myDFPlayer.next();
      playSomething();
    }
    
  }
}

//----------------------------------------------------------------------
// Buttons and logic
//----------------------------------------------------------------------

void playSomething() {
  if (isPlayingMusic) {
    logToChat(F("Playing music #") + String(lastMusicFile));
    myDFPlayer.playFolder(MUSIC_FOLDER, lastMusicFile);
  }
  else {
    logToChat(F("Playing sound #") + String(lastSoundFile));
    myDFPlayer.playFolder(SOUNDS_FOLDER, lastSoundFile);
  }
}

void refreshServerPage();

void checkTrackIds() {
  if (lastMusicFile==0)
    ++lastMusicFile;
  if (lastSoundFile==0)
    ++lastSoundFile;
}

void onWhite() {
  whiteEnabled = !whiteEnabled;
  redEnabled = false;
	blueEnabled = false;
  externalEnabled = false;
  refreshServerPage();
}

void onRed() {
  whiteEnabled = false;
  redEnabled = !redEnabled;
	blueEnabled = false;
  externalEnabled = false;
  refreshServerPage();
}

void onBlue() {
  whiteEnabled = false;
  redEnabled = false;
	blueEnabled = !blueEnabled;
  externalEnabled = false;
  refreshServerPage();
}

void onExternal()  {
  whiteEnabled = false;
  redEnabled = false;
	blueEnabled = false;
  externalEnabled = !externalEnabled;
  refreshServerPage();
}

void onPlayPause() {
  isPlaying = !isPlaying;
  if (!isPlaying) {
    logToChat(F("Player -> pause"));
    myDFPlayer.pause();
  } else {
    if (isJustStarted) {
      isJustStarted = false;
      isPlayingMusic ? ++lastMusicFile : ++lastSoundFile;
      
      logToChat(F("Player -> next"));
      //myDFPlayer.next();
      playSomething();
    }
    else {
      logToChat(F("Player -> start"));
      //myDFPlayer.start();
      playSomething();
    }
  }
  refreshServerPage();
}

void onNextTrack() {
  if (!isPlaying)
    isPlaying = true;
  isPlayingMusic ? ++lastMusicFile : ++lastSoundFile;

  logToChat(F("Player -> next"));
  //myDFPlayer.next();
  playSomething();

  refreshServerPage();
}

void onPrevTrack() {
  if (!isPlaying)
    isPlaying = true;
  isPlayingMusic ? --lastMusicFile : --lastSoundFile;
  checkTrackIds();

  logToChat(F("Player -> prev"));
  //myDFPlayer.previous();
  playSomething();

  refreshServerPage();
}

void onVolumeUp() {
  logToChat(F("Player -> volume Up"));
  myDFPlayer.volumeUp();
  myDFPlayer.volumeUp();
  refreshServerPage();
}

void onVolumeDown() {
  logToChat(F("Player -> volume Down"));
  myDFPlayer.volumeDown();
  myDFPlayer.volumeDown();
  refreshServerPage();
}

void onChangeFolder() {
  isPlayingMusic = !isPlayingMusic;
  playSomething();
  refreshServerPage();
}

//----------------------------------------------------------------------
// HTTP code
//----------------------------------------------------------------------

void onConnect() {
  server.send(200, "text/html", SendHTML()); 
}

void onNotFound(){
  server.send(404, "text/plain", "Not found");
}

void refreshServerPage() {
  //EEPROM.write(1, whiteEnabled);
  //EEPROM.write(2, redEnabled);
  //EEPROM.write(3, blueEnabled);
  //EEPROM.write(4, externalEnabled);  
  //EEPROM.commit();
  //logToChat("upd");
  server.send(200, "text/html", SendHTML());   
}

String SendHTML() {
  String ptr = "<!DOCTYPE html> <html lang=\"ru-RU\">";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\" charset=\"UTF-8\">";
  ptr += "<title>Подводна лодка</title>";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  ptr += "body{margin-top: 25px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 25px;}";
  ptr += ".button {display: block;width: 95px;background-color: #1abc9c;border: none;color: white;padding: 7px 15px;text-decoration: none;font-size: 20px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}";
  ptr += ".button-small {display: block;width: 60px;background-color: #1abc9c;border: none;color: white;padding: 7px 15px;text-decoration: none;font-size: 20px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}";
  ptr += ".button-on {background-color: #1abc9c;}";
  ptr += ".button-on:active {background-color: #16a085;}";
  ptr += ".button-off {background-color: #34495e;}";
  ptr += ".button-off:active {background-color: #2c3e50;}";
  ptr += "table, td {border:none;}";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}";
  ptr += "</style></head>";
  ptr += "<body><h2>Подводная лодка</h2>";

  ptr += "<table border=\"1\" cellpadding=\"4\" cellspacing=\"0\" align=\"center\">";

  // first row - leds
  ptr+="<tr>";
  ptr += whiteEnabled ?
    "<td><a class=\"button button-off\" href=\"/w\">Белый</a></td>"
    :
     "<td><a class=\"button button-on\" href=\"/w\">Белый</a></td>";

  ptr += redEnabled ?
    "<td><a class=\"button button-off\" href=\"/r\">Красный</a></td>"
    :
    "<td><a class=\"button button-on\" href=\"/r\">Красный</a></td>";
  ptr+="</tr>";

  // second row - leds  
  ptr += "<tr>";
  ptr += blueEnabled ?
    "<td><a class=\"button button-off\" href=\"/b\">Синий</a></td>"
    :
    "<td><a class=\"button button-on\" href=\"/b\">Синий</a></td>";

  ptr += externalEnabled ?
    "<td><a class=\"button button-off\" href=\"/e\">Ходовые</a></td>" 
    :
    "<td><a class=\"button button-on\" href=\"/e\">Ходовые</a></td>";
  ptr+="</tr>";

  // third row - current track
  ptr += "<tr>";
  ptr += "<td colspan=\"2\"><marquee>\"Очень-очень-очень-очень-очень длинное название трека\"</marquee></td>";
  ptr += "</tr>";

  // fourth row - play/pause, music/sounds
  ptr+="<tr>";

  ptr += isPlaying ?
    "<td><a class=\"button button-on\" href=\"/playPause\">Пауза</a></td>"
    :
    "<td><a class=\"button button-off\" href=\"/playPause\">Воспроизвести</a></td>";

  ptr += isPlayingMusic ?
    "<td><a class=\"button button-on\" href=\"/changeFolder\">Музыка</a></td>"
    :
    "<td><a class=\"button button-off\" href=\"/changeFolder\">Звуки</a></td>";

  ptr+="</tr>";

  // fith row - next/prev track
  ptr += "<tr>";
  ptr +="<td><a class=\"button-small button-off\" href=\"/nextTrack\"><<</a></td>";
  ptr +="<td><a class=\"button-small button-off\" href=\"/prevTrack\">>></a></td>";
  ptr += "</tr>";

  // sixth row - volume down/up
  ptr += "<tr>";
  ptr +="<td><a class=\"button-small button-off\" href=\"/volumeDown\">-</a></td>";
  ptr +="<td><a class=\"button-small button-off\" href=\"/volumeUp\">+</a></td>";
  ptr += "</tr>";

  ptr += "</table></body></html>";
  return ptr;
}

//---------------------
// Player

void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      logToChat(F("Time Out!"));
      break;
    case WrongStack:
      logToChat(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      logToChat(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      logToChat(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      logToChat(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      logToChat("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      logToChat("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      logToChat(F("Number:"));
      logToChat(String(value));
      logToChat(F(" Play Finished!"));
      break;
    case DFPlayerError:
      logToChat(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          logToChat(F("Card not found"));
          break;
        case Sleeping:
          logToChat(F("Sleeping"));
          break;
        case SerialWrongStack:
          logToChat(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          logToChat(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          logToChat(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          logToChat(F("Cannot Find File"));
          break;
        case Advertise:
          logToChat(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

