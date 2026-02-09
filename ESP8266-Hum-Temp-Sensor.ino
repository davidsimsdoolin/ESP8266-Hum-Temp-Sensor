#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

AsyncWebServer server(80);

String g_wifi[] = {"",""};
String g_interimWifi[] = {"",""};
String status = "disconnected";
String connectionText = "";
bool newConnectionRequest = false;

unsigned long lastScanTime = 0;

String networkList = "[]";
String sensorData = "[]";

Adafruit_SSD1306 display(128,  64, &Wire, -1);
DHT dht(2,  DHT11);

void sendSensor()
{
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  display.setTextSize(1);

  display.setCursor(0, 5);
  display.println("Wifi: " + status);
  display.setCursor(0, 18);
  display.println("IP:   " + WiFi.localIP().toString());

  display.setCursor(0, 35);
  display.println("Temperature:");
  display.setCursor(75, 35);
  display.println(String(temp) + " C");

  display.setCursor(0,  48);
  display.println("Humidity:");
  display.setCursor(75,  48);
  display.println(String(hum) + " %");


  sensorData = "{\"temperature\":\"" + String(temp) + "\", \"humidity\": \"" + String(hum) + "\"}";
}

//page not found
void notFound(AsyncWebServerRequest *request) 
{
  request->send(404, "text/plain", "Not found");
}

//fills a list of networks and returns count.
int getNetworks()
{
  int n = WiFi.scanNetworks();
  String networks[24];
  for (int x=0; x<n; x++)
  {
    networks[x] = WiFi.SSID(x);
  }
  return n;
}

void parseWifi(String wifiCS)
{ 
  int comma = wifiCS.indexOf(",");
  g_wifi[0] = wifiCS.substring(0,comma);
  g_wifi[1] = wifiCS.substring(comma+1, wifiCS.length());
}

//read data from file
String readFile(fs::FS &fs, const char * path)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory())
  {
    Serial.println("- empty file or failed to open file");
    return String();
  }

  String fileContent;
  while(file.available())
  {
    fileContent+=String((char)file.read());
  }
  file.close();
  return fileContent;
}

//write given data to file
void writeFile(fs::FS &fs, const char * path, const char * message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } 
  else 
  {
    Serial.println("- write failed");
  }
  file.close();
}

//Attempt to connect to Wifi
bool wifiConnect(String wifi[])
{
  WiFi.begin(wifi[0], wifi[1]);

  unsigned long connectTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectTime < 10000)
    delay(100);

  if(WiFi.status() == WL_CONNECTED)
  {
    status = "connected";
    return true;
  }
  else
  {
    status = "disconnected";
    return false;
  }
} 

//Load all pages for the site.
void startServer()
{
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/networks", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "application/json", networkList);
  });

  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "application/json", sensorData);
  });

  server.on("/status", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/plain", status);
  });

  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request) 
  {

    if (request->hasParam("ssid", true) && request->hasParam("password", true)) 
    {
      g_interimWifi[0] = request->getParam("ssid", true)->value();
      g_interimWifi[1] = request->getParam("password", true)->value();

      newConnectionRequest = true;
      connectionText = "Connecting to Wifi Network: " +interimWifi[0];
    }
    request->send(200, "text/plain", connectionText);
  });
  server.onNotFound(notFound);
  server.begin();
}


void setup() {
  Serial.begin(115200);

  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) 
  { // Address  0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
  }
  
  // Start LittleFS
  if(!LittleFS.begin())
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  //Get Saved Wifi Details
  parseWifi(readFile(LittleFS, "/ssid.txt"));
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.softAP("ESP" + WiFi.macAddress())
  wifiConnect(g_wifi);
  startServer();
}

//primary loop
void loop() 
{

  if (millis() - lastScanTime > 10000) 
  { 
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n && i < 20; i++) {
        if (i) json += ",";
        json += "\"" + WiFi.SSID(i) + "\"";
    }
    json += "]";
    networkList = json;
    lastScanTime = millis();
  }

  //bool set by if html submitted ssid and password. If true tries to connect.
  if(newConnectionRequest)
  {
    if(wifiConnect(interimWifi))
    {
      //if connection is succesful then saves details.
      g_wifi[0] = g_interimWifi[0];
      g_wifi[1] = g_interimWifi[1];
      writeFile(LittleFS, "/ssid.txt", (g_wifi[0] + "," + g_wifi[1]).c_str());
      connectionText = "Connected successfully to " + g_wifi[0];
    }
    else
    {
      connectionText = "Failed to connect to " + g_interimWifi[0];
    }
    newConnectionRequest = false;
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setFont();
  delay(2000);
  sendSensor();
  display.display(); 
}