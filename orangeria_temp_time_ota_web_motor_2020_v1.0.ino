
// gt orangeria program 2020

#define TCP_MSS whatever
#define LWIP_IPV6 whatever
#define LWIP_FEATURES whatever
#define LWIP_OPEN_SRC whatever
 
 
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <Timezone.h>
#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>


AsyncWebServer wserver(80);

int yourOFFtime;
int yourONtime;
float yourTemp;
const char* param_off = "OFFtime";
const char* param_on = "ONtime";
const char* param_temp = "Temp";
const char* param_radiobutton = "radiobutton";

time_t t;
time_t prevDisplay = 0; // when the digital clock was displayed
char t_time[10];
float tem;
float hum;
float temp[6];
float huma[6];
float pre[6];
int vent=0;
int ONmotor,OFFmotor;
float vent_temp=26.0;
int ONtimer,OFFtimer;
int state=1; // 1- автом, 2 вкл. помпа, 3 изкл.помпа
bool ONOFFmotor=false;
bool NIGHTmode=false;

// HTML web page to handle 3 input fields (OFFtime, ONtime, Temp)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>GT green house</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta content='text/html; charset=UTF-8' http-equiv='Content-Type'>
  <meta http-equiv="refresh" content="25">
  </head>
      <style>
      </style>
  <body>
  <h1>Оранжерия контрол</h1>
  <form action="/get" target="hidden-form">
    Изключена помпа (мин): <input type="number" value=%OFFtime% name="OFFtime">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get" target="hidden-form">
    Включена помпа (мин): <input type="number" value=%ONtime% name="ONtime">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get" target="hidden-form">
    Температура за вентилиране: <input type="number" value=%Temp% name="Temp">
    <input type="submit" value="Submit">
  </form>
  <iframe style="display:none" name="hidden-form"></iframe>
  <p>ESP8266 часовник: %Time% Текуща температура: %Ttemp% &#8451;</p>
  <p>РЕЖИМ НА РАБОТА: <strong> %STATE%</strong></p>
  <p>----------- ПРИ АВТОМАТИЧЕН РЕЖИМ -------------- </p>
  <p> Работа ДЕН/НОЩ: %Regim% </p>
  <p> Вентилатор в момента:  %Vstatus%
  <p> Помпа в момента: %Pstatus%
  <p> Вкл.време: %ontimer% Изкл.време %offtimer% </p>
  <p>-------------------------------------------------------------- </p>
    
  <form action="/get" target="hidden-form">
  <input type="radio" id="auto" name="radiobutton" value="1">
  <label for="auto">Автоматичен</label>
  <input type="radio" id="pumpON" name="radiobutton" value="2">
  <label for="pumpON">Помпа ВКЛ.</label>
  <input type="radio" id="pumpOFF" name="radiobutton" value="3">
  <label for="pumpOFF">Помпа ИЗКЛ.</label><br>
  <input type="submit" value="Submit">
  </form>

</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Replaces placeholder with stored values
String processor(const String& var){
  //Serial.println("new ");
  if(var == "OFFtime"){
    return readFile(SPIFFS, "/OFFtime.txt");
  }
  else if(var == "ONtime"){
    return readFile(SPIFFS, "/ONtime.txt");
  }
  else if(var == "Temp"){
    return readFile(SPIFFS, "/Temp.txt");
  }
   else if(var == "Time"){
    sprintf( t_time, "%02hhu:%02hhu", hour(t), minute(t));
    return t_time;
  }
   else if(var == "Ttemp"){
     return String(tem);
  }
  else if(var == "Vstatus"){
     if (vent==1)
        {
          return "<span style='color:green'> ВЛЮЧЕН </span></p>"; 
        }
        else
        {
         return "<span style='color:red'> ИЗКЛЮЧЕН </span></p>"; 
        }
  }
  else if(var == "Pstatus"){
     if (ONOFFmotor && !NIGHTmode)
        {
          return "<span style='color:green'> ВЛЮЧЕНА </span></p>"; 
        }
        else
        {
         return "<span style='color:red'> ИЗКЛЮЧЕНА </span></p>"; 
        }
  }
   else if(var == "Regim"){
     if (NIGHTmode)
        {
          return "<span style='color:red'> НОЩЕН режим (изключено всичко) </span></p>"; 
        }
        else
        {
         return "<span style='color:green'> ДНЕВЕН режим </span></p>"; 
        }
  }
  else if(var == "ontimer"){
     return String(ONtimer);
  }
  else if(var == "offtimer"){
     return String(OFFtimer);
  }
  else if(var == "STATE"){
    if (state==1)
        {
          return "<span style='color:blue'> АВТОМАТИЧЕН РЕЖИМ </span>"; 
        }
    else if (state==2)
        {
         return "<span style='color:green'> ПОМПА ВКЛЮЧЕНА </span>"; 
        }
    else if (state==3)
    {
         return "<span style='color:red'> ПОМПА ИЗКЛЮЧЕНА </span>"; 
    }
  }
  
  return String();
}

static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 0;     //UTC
WiFiUDP Udp;
unsigned int localPort = 2390;   // local port to listen for UDP packets
// Eastern European Time (Sofia)
TimeChangeRule EEST = {"CEST", Last, Sun, Mar, 2, 180};     // Central European Summer Time
TimeChangeRule EET = {"CET ", Last, Sun, Oct, 3, 120};       // Central European Standard Time
Timezone EE(EEST, EET);


time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

WiFiClient client; 

// replace with your channel's thingspeak API key, 
String apiKey = "6GHKC73UDEKMT781";
//const char* ssid = "Genadi-N";
const char* ssid = "GENADI-HOME2";
const char* password = "*******";

const char* server = "184.106.153.149";   //"184.106.153.149" or api.thingspeak.com

#define DHTPIN D4 // what pin we're connected D4
#define VENTPIN D2 //D2
#define MOTORPIN D3 
DHT dht(DHTPIN, DHT22);

   
//------------------------------------------------------------




//Bubble sort my ar*e
void isort(float *a, int n)
{
 for (int i = 1; i < n; ++i)
 {
   float j = a[i];
   int k;
   for (k = i - 1; (k >= 0) && (j < a[k]); k--)
   {
     a[k + 1] = a[k];
   }
   a[k + 1] = j;
 }
}



//------------------------------------------------------------

void setup() {    
        
  Serial.begin(115200);
  delay(250);
   if(!SPIFFS.begin()){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
    
  WiFi.begin(ssid, password);
 
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
   
  WiFi.begin(ssid, password);
   
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  delay(250);

  Udp.begin(localPort);
  Serial.print("Local port for UDP: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(60*30); //секунди за синхронизиране с нтп
  
  pinMode(VENTPIN,OUTPUT);
  digitalWrite(VENTPIN,LOW);
  pinMode(MOTORPIN,OUTPUT);
  digitalWrite(MOTORPIN,LOW);
//pri pyrvo startirane.
//writeFile(SPIFFS, "/OFFtime.txt","100");
//writeFile(SPIFFS, "/ONtime.txt", "15");
//  writeFile(SPIFFS, "/Temp.txt", "32");  

  // ota begin
  //ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("orangeriaes8266");
 ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

// Send web page with input fields to client
  wserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/get?OFFtime=<inputMessage>
  wserver.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
//*----------------------------------------test на параметрите
int params = request->params();
Serial.println(params);
for(int i=0;i<params;i++){
  AsyncWebParameter* p = request->getParam(i);
  if(p->isFile()){ //p->isPost() is also true
    Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
  } else if(p->isPost()){
    Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
  } else {
    Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
  }
} 
//*/ //----------------------------------------край test на параметрите
    // GET OFFtime value on <ESP_IP>/get?OFFtime=<inputMessage>
    if (request->hasParam(param_off)) {
      inputMessage = request->getParam(param_off)->value();
      writeFile(SPIFFS, "/OFFtime.txt", inputMessage.c_str());
      OFFmotor=inputMessage.toInt();
      OFFtimer=OFFmotor;
    }
    // GET ONtime value on <ESP_IP>/get?ONtime=<inputMessage>
    else if (request->hasParam(param_on)) {
      inputMessage = request->getParam(param_on)->value();
      writeFile(SPIFFS, "/ONtime.txt", inputMessage.c_str());
      ONmotor=inputMessage.toInt();
      ONtimer=ONmotor;
    }
    // GET Temp value on <ESP_IP>/get?Temp=<inputMessage>
    else if (request->hasParam(param_temp)) {
      inputMessage = request->getParam(param_temp)->value();
      writeFile(SPIFFS, "/Temp.txt", inputMessage.c_str());
      vent_temp=inputMessage.toFloat();
    }
     
    // GET pumpON on <ESP_IP>/get?pumpON=<inputMessage>
    else if (request->hasParam(param_radiobutton)) {
      AsyncWebParameter* p = request->getParam(0);
      
      state=p->value().toInt();
      Serial.print("State:");
      Serial.print(state);
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
  });
  
  wserver.onNotFound(notFound);
  wserver.begin();


  
  Serial.println("Web server started...");
  OFFmotor= readFile(SPIFFS, "/OFFtime.txt").toInt();
  ONmotor= readFile(SPIFFS, "/ONtime.txt").toInt();
  vent_temp= readFile(SPIFFS, "/Temp.txt").toFloat();
  OFFtimer=OFFmotor;
  ONtimer=ONmotor;
  Serial.println("Init end...");
 }
//--------------------------------------------------------------end Setup  
  
 
void loop() {
  ArduinoOTA.handle();
  
   if (second() == 0)
   //-------------------------начало на изпращането към tingspeak
    { // thingspeak needs minimum 60 sec delay between updates
          Serial.println("Begin.");
          if (timeStatus() != timeNotSet) {
            if (now() != prevDisplay) { //update the display only if time has changed
               prevDisplay = now();
              }
          }

          digitalClockDisplay();
          int readings = 5;
          Serial.print(readings);
          Serial.println(" temperature read."); 
          while( readings >= 0)
          {
          
          delay(300);   
          dht.begin();
          delay(100);
          tem = dht.readTemperature();
          
          delay(300);   
          dht.begin();
          delay(100);
          hum = dht.readHumidity();
      
            temp[readings]=tem;
            huma[readings]=hum; 
            if (!isnan(tem)&& !isnan(hum) ) {
              readings--;
              Serial.print(readings);
            }
           
          }
          Serial.println(" End reading.");
          isort(temp,6);
          isort(huma,6);
          tem=(temp[2]+temp[3])/2;
          hum=(huma[2]+huma[3])/2;
          Serial.println("Send data to thngspeak."); 
          if (client.connect(server,80)) {  //   "184.106.153.149" or api.thingspeak.com
           String postStr = apiKey;
           postStr +="&field4=";
           postStr += String(tem);
           postStr +="&field5=";
           postStr += String(hum);
           postStr += "\r\n\r\n";
 
          client.print("POST /update HTTP/1.1\n"); 
          client.print("Host: api.thingspeak.com\n"); 
          client.print("Connection: close\n"); 
          client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n"); 
          client.print("Content-Type: application/x-www-form-urlencoded\n"); 
          client.print("Content-Length: "); 
          client.print(postStr.length()); 
          client.print("\n\n"); 
          client.print(postStr);
           
 
          Serial.print("Temperature: ");
          Serial.print(tem);
          Serial.print(" Humidity: "); 
          Serial.println(hum);
          client.stop();
          Serial.println("Data send."); 
        }
//-------------------------край на изпращането към tingspeak

//-------------------------Релета начало 
if (state==1)   // автоматичен режим
{      
     
      if (ONOFFmotor)
      {
        ONtimer--;
        if (ONtimer==0)
          {
            ONOFFmotor=false;
            ONtimer=ONmotor;
          }
      }
      else
      {
        OFFtimer--;
        if (OFFtimer==0)
        {
          ONOFFmotor=true;
          OFFtimer=OFFmotor;
            
        }
      }
       if (ONOFFmotor)
        Serial.println("auto Pump on");
       else
        Serial.println("auto Pump off");
      
} // край на автоматичен режим

Serial.println("Relays end");
Serial.print("State: ");
Serial.println(state);
//-------------------------Релета край    
  } //end if на кръгла минута


if (state==1)
{
   
    if (tem>vent_temp) 
          {
            vent=1; //включва вентилатора
            digitalWrite(VENTPIN,HIGH);
            //Serial.println("auto Vent on");
          }
      else
        {
          vent=0; //изкл. вентилатора
          digitalWrite(VENTPIN,LOW);
          //Serial.println("auto Vent off");
        }

    if (!ONOFFmotor)
          {
            // изключване на помпата
            digitalWrite(MOTORPIN,LOW);
          }
          else
          {
             // включване на помпата
            if (hour(t)>=22 || hour(t)<=6) {
              //Serial.println("Night off for silence!");
              NIGHTmode=true;
              digitalWrite(MOTORPIN,LOW);
           }
            else
            {
              NIGHTmode=false;
              digitalWrite(MOTORPIN,HIGH);
            }
          }
}
  
if (state==2) // включена помпа, вентилатор изключен
{
   digitalWrite(MOTORPIN,HIGH);
   //Serial.println("Pump on");
   digitalWrite(VENTPIN,LOW);
   //Serial.println("Vent off");
}
if (state==3)// изключена помпа, вентилатор изключен
{
  digitalWrite(MOTORPIN,LOW);
   //Serial.println("Pump off");
   digitalWrite(VENTPIN,LOW);
   //Serial.println("Vent off");
}
delay(100);
}

void digitalClockDisplay()
{
  // digital clock display of the time
  TimeChangeRule *tcr;        // pointer to the time change rule
  t = EE.toLocal(prevDisplay, &tcr);
  Serial.print(hour(t));
  printDigits(minute(t));
  printDigits(second(t));
  Serial.print(" ");
  Serial.print(day(t));
  Serial.print(".");
  Serial.print(month(t));
  Serial.print(".");
  Serial.print(year(t));
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}



/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 2208988800UL; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
