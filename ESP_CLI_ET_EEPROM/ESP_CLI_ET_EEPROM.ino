#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

#include <EEPROM.h>


#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Servo.h>
#include <ArduinoJson.h>
 
//Variables
int i = 0;
int statusCode;
const char* ssid = "text";
const char* passphrase = "text";
String st;
String content;

const int btn1 = 12;
boolean btnapp = 0;

//anti-rebond
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
 
//Function Decalration
bool testWifi(void);
void launchWeb(void);
void setupAP(void);
 
//init serveur web sur port 80
ESP8266WebServer server(80);

SocketIOclient socketIO;
 
void setup()
{
 
  Serial.begin(230400);
  Serial.println();
  Serial.println("Déconnexion du réseau WIFI précédent.");
  WiFi.disconnect();

  
  EEPROM.begin(512); //Init EEPROM

  
  delay(10);
  //pinMode(LED_BUILTIN, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(btn1, INPUT_PULLUP);
  Serial.println();
  Serial.println();
  Serial.println("Démarrage");
 
  //---------------------------------------- EEPROM SSID
  Serial.println("Lecture du SSID dans EEPROM");
  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }

  
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);

  //---------------------------------------- EEPROM PASS
  Serial.println("Lecture du pass dans EEPROM");
  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }

  
  Serial.print("PASS: ");
  Serial.println(epass);
 
 
  WiFi.begin(esid.c_str(), epass.c_str());
  if (testWifi())
  {
    Serial.println("Connecté à la WIFI avec EEPROM. Demarrage du socket...");
    
    // server address, port and URL
    socketIO.begin("100.64.98.56", 3000, "/socket.io/?EIO=4");

    // event handler
    socketIO.onEvent(socketIOEvent);
    return;
  }
  else
  {
    Serial.println("Aucune connexion par EEPROM : Démarrage du HotSpot");
    //Serveur web
    launchWeb();
    //Point d'accès
    setupAP();
  }
 
  Serial.println();
  Serial.println("En attente");
  
  while ((WiFi.status() != WL_CONNECTED))
  {
    Serial.print(".");
    delay(1000);
    server.handleClient();
  }
 
}
void loop() {
  socketIO.loop();
  boolean etatbtn = digitalRead(btn1);

  if (etatbtn != btnapp) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    
    if(btnapp == 0 and etatbtn == 0){
      btnapp = 1;
      DynamicJsonDocument boutonpoussoir(1024);
      JsonArray arraybouton = boutonpoussoir.to<JsonArray>();
      arraybouton.add("btnphys");
      
      JsonObject param1 = arraybouton.createNestedObject();
      param1["btnphys"] = "databtn";
      
      // JSON to String (serializion)
      String output;
      serializeJson(boutonpoussoir, output);
      
      // Send event
      socketIO.sendEVENT(output);
    
    } else if(btnapp == 1 and etatbtn == 1) {
      btnapp = 0;
    }
  }
}
 
 bool testWifi(void)
{
  int c = 0;
  Serial.println("En atente de connexion WIFI");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Timeout, démarrage HotSpot");
  return false;
}
 
void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("Connecté à la WIFI");
  Serial.print("HotSpot IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Démarage du serveur web
  server.begin();
  Serial.println("Serveur web démarré");
}
 
void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("Scan effectué.");
  if (n == 0)
    Serial.println("Aucun réseau trouvé.");
  else
  {
    Serial.println("Des réseaux ont été trouvés : consulter la page web.");
  }
  
  Serial.println("");
  
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
  // SSID + PASS de chaque réseau
    st += "<li>";
    st += WiFi.SSID(i);
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  
  WiFi.softAP("WIFI ESP8266 Theodor", "");
  launchWeb();
  Serial.println("Terminé");
}
 
void createWebServer()
{
 {
    server.on("/", []() {
 
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>ESP8266 (" + ipStr + ") en attente d une connexion a un reseau WIFI.";
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });

    //vidage et écriture dans l'EEPROM
    server.on("/setting", []() {
      
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      
      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");
 
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        }
        EEPROM.commit();
 
        content = "{\"Success\":\"Enregistrement dans EEPROM fait... Reboot et demarrage sur nouveaux parametres WIFI.\"}";
        statusCode = 200;
        ESP.reset();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Envoi 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
 
    });
  }
}

void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length) {
    switch(type) {
        case sIOtype_DISCONNECT:
            Serial.print("[IOc] Disconnected!\n");
            break;
        case sIOtype_CONNECT:
            Serial.printf("[IOc] Connected to url: %s\n", payload);

            // join default namespace (no auto join in Socket.IO V3)
            socketIO.send(sIOtype_CONNECT, "/");
            break;

            
        case sIOtype_EVENT:
            Serial.printf("[IOc] get event: %s\n", payload);
            handleWebSocketMessage(String((char *) payload));
            break;
      
        case sIOtype_ACK:
            Serial.printf("[IOc] get ack: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_ERROR:
            Serial.printf("[IOc] get error: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_EVENT:
            Serial.printf("[IOc] get binary: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_ACK:
            Serial.printf("[IOc] get binary ack: %u\n", length);
            hexdump(payload, length);
            break;
    }
}

void handleWebSocketMessage(String data) {
  if(data == "[\"true\"]"){
    digitalWrite(15, HIGH);
    digitalWrite(14, LOW);
    //digitalWrite(LED_BUILTIN, LOW);
  }
  if(data == "[\"false\"]"){
    digitalWrite(14, HIGH);
    digitalWrite(15, LOW);
    //digitalWrite(LED_BUILTIN, HIGH);
  }
}
