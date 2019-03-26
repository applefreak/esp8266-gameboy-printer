#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>

#define MISO 12
#define MOSI 13
#define SCLK 14

void processData(uint8_t data);
void storeData(uint8_t *image_data);
uint32_t getImageCount();

uint8_t clock_count = 0x00;
uint8_t current_data = 0x00;

uint32_t packet_count = 0x00;
uint32_t packet_legth = 0x00;
uint8_t current_packet_type = 0x00;
bool printed = false;
uint8_t inquiry_count = 0x00;

uint8_t image_data[11520] = {};
uint32_t img_index = 0;
ESP8266WebServer server(80);

void gbClockHit() {
  if (digitalRead(MOSI) == HIGH) {
    current_data |= 0x01;
  }

  if (packet_count == (packet_legth - 3)) {
    if (clock_count == 7) {
      digitalWrite(MISO, HIGH);
    }
  }
  if (packet_count == (packet_legth - 2)) {
    if (clock_count == 0 || clock_count == 7) {
      digitalWrite(MISO, LOW);
    } else if (clock_count == 6) {
      digitalWrite(MISO, HIGH);
    }
  }

  if (clock_count == 7) {
    processData(current_data);
    clock_count = 0;
    current_data = 0x00;
  } else {
    current_data = current_data << 1;
    clock_count++;
  }
}

void processData(uint8_t data) {
  if (packet_count == 2) { //command type
    current_packet_type = data;
    switch (data) {
      case 0x04:
      packet_legth = 0x28A; // 650 bytes
      break;

      case 0x02:
      packet_legth = 0x0E; // 14 bytes
      break;

      default:
      packet_legth = 0x0A; // 10 bytes
      break;
    }
  }

  // Handles that special empty body data packet
  if ((current_packet_type == 0x04) && (packet_count == 4) && (data == 0x00)) {
    packet_legth = 0x0A;
  }

  if ((current_packet_type == 0x04) && (packet_count >= 6) && (packet_count <= packet_legth - 5)) {
    image_data[img_index++] = data;
  }

  if (current_packet_type == 0x02) {
    printed = true;
  }

  if (printed && (packet_count == 2) && (data == 0x0F)) {
    inquiry_count++;
  }

  if (packet_count == (packet_legth - 1)) {
    packet_count = 0x00;
    if (inquiry_count == 4) {
      storeData(image_data);
    }
  } else {
    packet_count++;
  }
}

void storeData(uint8_t *image_data) {
  detachInterrupt(14);
  // Serial.println("Storing...");

  uint32_t img_count = getImageCount();
  img_count++;

  File f = SPIFFS.open("/img/img" + String(img_count) + ".dat", "w");

  if (!f) {
    Serial.println("file creation failed");
  }

  f.write(image_data, img_index);

  f.close();

  // Serial.println("Image saved! Attaching Interrupt...");
  clock_count = 0x00;
  current_data = 0x00;
  packet_count = 0x00;
  packet_legth = 0x00;
  current_packet_type = 0x00;
  printed = false;
  inquiry_count = 0x00;
  img_index = 0x00;
  attachInterrupt(14, gbClockHit, RISING);
  // Serial.println("Interrupt attached!");
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  Serial.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleImgList() {
  Dir dir = SPIFFS.openDir("/img");

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

uint32_t getImageCount() {
  Dir dir = SPIFFS.openDir("/img/");
  uint32_t result = 0;
  while(dir.next()) {
    result++;
  }
  return result;
}

void removeAllImages() {
  Dir dir = SPIFFS.openDir("/img");
  while(dir.next()){
    SPIFFS.remove(dir.fileName());
  }
  server.send(200, "text/json", "true");
}

void resetAllCounters() {
  Serial.println("Resetting All Counters...");
  clock_count = 0x00;
  current_data = 0x00;
  packet_count = 0x00;
  packet_legth = 0x00;
  current_packet_type = 0x00;
  printed = false;
  inquiry_count = 0x00;
  img_index = 0x00;
  Serial.println("All Counters Reseted");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup!");
  Serial.println();

  for(uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  // Setup ports
  pinMode(MISO, OUTPUT);
  pinMode(MOSI, INPUT);
  pinMode(SCLK, INPUT);

  // Setup Clock Interrupt
  attachInterrupt(14, gbClockHit, RISING);
  attachInterrupt(5, resetAllCounters, FALLING);
  Serial.println("End Setup!");
  Serial.println();

  Serial.println("Setting up Soft AP...");
  bool res = WiFi.softAP("GameboyPrinter");
  if (res) {
    Serial.println("Soft AP Up and Running!");
  }

  // Mount SPIFFS
  delay(500);
  SPIFFS.begin();
  delay(500);

  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });
  server.on("/list_img", HTTP_GET, handleImgList);
  server.on("/remove_all", HTTP_GET, removeAllImages);

  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  server.handleClient();
}
