#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <FS.h>
#include <SD_MMC.h>
#include <SPI.h>
#include "esp32cam.h"


Eloquent::Esp32cam::Cam cam;

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
    // Setting up pin 13 as input
  pinMode(GPIO_NUM_12, INPUT);


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
  server.on("/battery",handleBattery);
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
}

void loop() {
  server.handleClient();

  if ((digitalRead(GPIO_NUM_12) == HIGH)) {
    isMovement = true;
    movementCounter++;
  }

  if (isMovement && (movementCounter >= 3000)) {
    Serial.println("Movement Detected!");

    if (!cam.capture()) {
        Serial.println(cam.getErrorMessage());
        return;
    }

    String filename = String("/images/") + counter + ".jpg";

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


// Handles what happens if a client connects to the root directory
void handleRoot() {
  /* SD_MMC pertains to the sd card "memory". It is save as a
    variable at the same address given to fs in the fs library
    with "FS" class to enable the file system wrapper to make
    changes on the sd cards memory */
  fs::FS &fs = SD_MMC;
  String path = server.uri(); //saves the to a string server uri ex.(192.168.100.110/edit server uri is "/edit")
  Serial.print("path ");  Serial.println(path);

  //To send the index.html when the serves uri is "/"
  if (path.endsWith("/")) {
    path += "index.html";
  }

  //gets the extension name and its corresponding content type
  String contentType = getContentType(path);
  Serial.print("contentType ");
  Serial.println(contentType);
  File file = fs.open(path, "r"); //Open the File with file name = to path with intention to read it. For other modes see <a href="https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html" style="font-size: 13.5px;"> https://arduino-esp8266.readthedocs.io/en/latest/...</a>
  size_t sent = server.streamFile(file, contentType); //sends the file to the server references from <a href="https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/src/WebServer.h" style="font-size: 13.5px;"> https://arduino-esp8266.readthedocs.io/en/latest/...</a>
  file.close(); //Close the file
}

//This functions returns a String of content type
String getContentType(String filename) {
  if (server.hasArg("download")) { // check if the parameter "download" exists
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) { //check if the string filename ends with ".htm"
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}

void handleDelete() {
  String message = "Deleted all images!";
  server.send(200, "text/plain", message);
  listDir(SD_MMC, "/", 0);
  }

String batteryHTML = "<!DOCTYPE html>"
  "<head>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='X-UA-Compatible' content='IE=edge'>"
    "<title>Greenhouse</title>"
    "<meta name='description' content='group26'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<style>"
        ".center {"
            "margin: auto;"
            "text-align: center;"
        "}"
        ".left {"
           " float: left;"
        "}"
        ".right {"
            "float: right;"
        "}"
        ".card {"
            "border-style: solid;"
            "border-width: 1px;"
            "border-color: #F7DC6F;"
            "padding: 5px;"
           " margin: 20px;"
        "}"
       " .container {"
            "position: relative;"
        "}"
       " .top-left {"
            "position: absolute;"
           " top: 8px;"
           " left: 16px;"
           " text-align: left;"
        "}"
        ".hide {"
            "position: absolute;"
            "opacity: 0;"
       " }"
        "input[type=checkbox]+label {"
            "color: rgb(129, 129, 129);"
            "font-style: italic;"
            "padding: 5px;"
        "}"
        "input[type=checkbox]:checked+label {"
           " color: #000000;"
           " font-style: normal;"
           " padding: 5px;"
        "}"
   " </style>"
"</head>"
"<body class='center' onload='onload()' style='font-family: Verdana, Geneva, Tahoma, sans-serif;'>"
    "<h1 class='center'>Group 26 Wireless Camera Trap</h1>"
    "<div class=card>"
        "<div style='background-color:#F7DC6F;'>"
            "Battery Status"
        "</div>"
            "<div>"
                "<p>The battery percentage is: ";



  void handleBattery() {
    int battAnalogRead = analogRead(GPIO_NUM_12);
    Serial.println("Not Enough ADC pins!");

    String message = batteryHTML + battAnalogRead + "</p>\n</div>\n</div>\n</body>\n</html>";

  server.send(200, "text/html", message);
  }
