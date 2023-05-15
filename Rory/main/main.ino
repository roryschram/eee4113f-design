#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <FS.h>
#include <SD_MMC.h>
#include <SPI.h>
#include "esp32cam.h"
#include "esp32cam/http/LiveFeed.h"


Eloquent::Esp32cam::Cam cam;
Eloquent::Esp32cam::Http::LiveFeed feed(cam, 80);

uint32_t counter = 1;
bool isMovement = false;
int movementCounter = 0;

// Set these to your desired credentials.
const char *ssid = "group26";
const char *password = "12345678";

WebServer server(80);

// Handles what happens when a client trys to connect to a page that has not been found
void handleNotFound() {
  String message = "Page not found!\n";
  server.send(404, "text/plain", message);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}


void setup() {
  Serial.begin(115200);
  Serial.println();

  // Setting up pin 13 as input
  pinMode(GPIO_NUM_13, INPUT);


  Serial.println("Setting Up Access Point");

  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while(1);
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/", handleRoot);
  server.on("",handleRoot);
  server.on("/delete",handleDelete);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Server started");


  // Setup the camera module
  // see 3_Get_Your_First_Picture for more details
  cam.aithinker();
  cam.highQuality();
  cam.vga();

  while (!cam.begin()) {
    Serial.println(cam.getErrorMessage());
  }
    
  // Initialize the filesystem
  // If something goes wrong, print an error message
  while (!SD_MMC.begin("/sdcard",true) || SD_MMC.cardType() == CARD_NONE) {
    Serial.println("Cannot init SD Card");
    delay(1000);
  }

  uint8_t cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No card attached");
    return;
  }
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  Serial.println("Enter 'capture' in the Serial Monitor");

}

void loop() {
  server.handleClient();

  if ((digitalRead(GPIO_NUM_13) == HIGH)) {
    isMovement = true;
    movementCounter++;
  }

  if (isMovement && (movementCounter >= 1000)) {

    if (!cam.capture()) {
        Serial.println(cam.getErrorMessage());
        return;
    }

    String filename = String("/picture_") + counter + ".jpg";

    if (cam.saveTo(SD_MMC, filename)) {
        Serial.println(filename + " saved to disk");
        counter += 1;
    }
    else {
        Serial.println(cam.getErrorMessage());
    }
    isMovement = false;
    movementCounter = 0;
  }
}


// HTML & CSS contents which display on web server
String HTML = "<!DOCTYPE html>\
<html>\
<body>\
<h1>My First Web Server with ESP32 - Station Mode &#128522;</h1>\
</body>\
</html>";

// Handles what happens if a client connects to the root directory
void handleRoot() {
  server.send(200, "text/html", HTML);
}

void handleDelete() {
  String message = "Deleted all images!";
  server.send(200, "text/plain", message);
  listDir(SD_MMC, "/", 0);
  }
