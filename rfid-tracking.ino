/* RFiD Attendance Logger
 *  Author: Lilis Athanasios
 *  Hardware: Arduino Mega2560, Ethernet Shield, LCD 16x2, tinyRTC, bluetooth module, RFID RC522 module
 */

#include <LiquidCrystal.h>
#include <Ethernet.h>
#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#define SS_PIN 53 //slave select pin for rfid
#define RST_PIN 46 //reset select pin for rfid

int buzzer = 5;  // speaker or buzzer on pin 5
int led_green = 4; // green LED on pin 4
int led_red = 3; // red LED on pin 3
int timeStamp[6] = {0, 0, 0, 0, 0, 0}; //initiate input values for timestamp
//char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(22, 24, 26, 28, 30, 32);
RTC_DS1307 rtc; //create RTC instace
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance.
File myFile; //file instance
/*
  //set ethernet shield
  byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
  };
  IPAddress ip(195, 251, 56, 63);//4homeUse: 192, 168, 1, 5
  IPAddress myDns(195, 251, 56 ,1);//4homeUse: 192, 168, 1, 1
  IPAddress gateway(195, 251, 56 ,1);//4homeUse: 192, 168, 1, 1
  IPAddress subnet(255, 255, 255, 128);//4homeUse: 255, 255, 255, 0
  // Initialize the Ethernet server library with the IP address and port you want to use
  EthernetServer server(80); // (port 80 is default for HTTP)
  EthernetClient client = server.available();  // check for HTTP request
*/

DateTime arrival[2];  // tiem class for arrival
DateTime departure[2];  // time class for departure
int LastMonth = 0; 
char DataRead = 0; // for character reading from a file
unsigned int MinsA = 0, HoursA = 0;  // working minutes and hours for tag A

//0 for arrival 1 for departure
String knownUID[8] = {
  "d699d1b5", "0", //"string1","string2"
  "d704e4d5", "0", //"string3","string4"
  "25372f00", "0", //"string5","string6"
  "fc663400", "0", //"string7","string8"
};
String readTag = ""; // variable for making hex byte tag to string
int flag; // possition of known tag on array
String folder="";
//---------------------------------------------------SETUP SECTION-----------------------------------------------------
void setup() {
  Serial.begin(57600);
  while (!Serial) {
     // -----------------------SOS----> ONLY for DEBUG
  };
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Debbuging mode");
  SPI.begin();
  pinMode(10, OUTPUT);
  pinMode(53, OUTPUT);
  pinMode(4, OUTPUT);
  
  //-------------initialize SD card-------------------------------------------
  enableSd(); //select SD-card SPI-slave
  Serial.println("Initializing SD card...");
  if (!SD.begin(4)) {
    Serial.println("ERROR - SD card initialization failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card error OR");
    lcd.setCursor(0, 1);
    lcd.print("Missing card");
    for(;;){};// wait here for ever
  }
  else Serial.println("SUCCESS - SD card initialized.");
  
  //---------------------setting up the time --------------------------------
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    lcd.clear();
    lcd.print("Clock not found");
    for(;;){};// wait here for ever
  }
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    lcd.clear();
    lcd.print("Clock ERROR");
    Serial.println("Set clock automaticaly? 1 for Yes 0(zero) for No");
    while (Serial.available() == 0) {} //wait here until serial input
    int ansClock= Serial.parseInt(); //read from bluetooth/serial
    if(ansClock==0) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  else{
    lcd.clear();
    printTime();
    Serial.println("Is time correct? 1 for Yes 0(zero) for No");
    while (Serial.available() == 0) {} //wait here until serial input
    int ansTime= Serial.parseInt(); //read from bluetooth/serial
    if(ansTime==0) setTime(); 
  }//------------------------time is set-----------------------
  
  //----------start the Ethernet connection and the server:--------------------
  //enableEther(); //select Ethernet SPI-slave
  //Ethernet.begin(mac, ip, myDns, gateway, subnet);
  //server.begin();
  //Serial.print("server is at ");
  //Serial.println(Ethernet.localIP());
  //---------------initialize rfid connection-----------------------------
  mfrc522.PCD_Init(); // Init MFRC522 card
  Serial.println("Scan PICC to see UID..."); 
}
void loop() {
  redLED();
  printTime();
  //---------------Check for client requests-----------------
  //enableEther();
  //webServer();
  //--------------Look for new cards-------------------------
  int succesRead = getID(); // read RFID tag
  if (succesRead == 1) { // if RFID read was succesful
    Serial.println("card inserted:");
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    int knownID = checkID(mfrc522.uid.uidByte, mfrc522.uid.size);
    if (knownID == 1) { // if tag is known, green light is on and store data
      greenLED();
      StoreData(flag, readTag);
    }
    else { // beeb an error; if new tag, then exit
      error();
      Serial.println("you're unknown");
    }
  }
  delay(1000);
  lcd.clear();
}/*------------------------------------END OF LOOP-------------------------------------------------------------------------------*/

void enableSd() {
  //disable Ethernet chip
  digitalWrite(10, HIGH);
  //disable spi bus from RFiD
  digitalWrite(53, HIGH);
  //ENABLE spi bus for SD
  digitalWrite(4, LOW);
}
void enableEther() {
  //disable spi bus from RFiD
  digitalWrite(53, HIGH);
  //disable spi bus from SD
  digitalWrite(4, HIGH);
  //ENABLE spi bus for ethernet
  digitalWrite(10, LOW);
}
void enableRfid() {
  //disable Ethernet chip
  digitalWrite(10, HIGH);
  //disable spi bus from SD
  digitalWrite(4, HIGH);
  //ENABLE spi bus from RFiD
  digitalWrite(53, LOW);
}
//routine for setting up the time to RTC
void setTime() {
  Serial.println("setting Time :");
  Serial.println("Give me the year , month, day, hour, minute, second");
  for (int i = 0; i < 6; i++) {
    while (Serial.available() == 0) {} //wait here until serial input
    timeStamp[i] = Serial.parseInt(); //read from bluetooth/serial
    Serial.print("you've entered :");
    Serial.println((int)timeStamp[i]);
    delay(100);
  }
  //adjust time with this row : year , month, day, hour, minute, second
  rtc.adjust(DateTime(timeStamp[0], timeStamp[1], timeStamp[2], timeStamp[3], timeStamp[4], timeStamp[5]));
  return;
}
void printTime() {
  DateTime now = rtc.now();
  lcd.setCursor(0, 0);
  lcd.print(now.year(), DEC);
  lcd.print('/');
  lcd.print(now.month(), DEC);
  lcd.print('/');
  lcd.print(now.day(), DEC);
  lcd.setCursor(0, 1);
  lcd.print(now.hour(), DEC);
  lcd.print(':');
  lcd.print(now.minute(), DEC);
  lcd.print(':');
  lcd.print(now.second(), DEC);
}
//Helper routine to dump a byte array as hex values to Serial Monitor.
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
// Read RFID
int getID() {
  enableRfid();
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}
//Check RFiD Tag if is known
int checkID(byte *buffer, byte bufferSize) {
  readTag = "";
  for (byte i = 0; i < bufferSize; i++) {  //HEX tag TO string
    readTag = readTag + String(buffer[i] < 0x10 ? "0" : "");
    readTag = readTag + String(buffer[i], HEX);
  }
  for (int j = 0; j < 8; j += 2) { //check if Tag is known
    if (readTag == knownUID[j]) {
      flag = j + 1; //return possition of flag
      //+1 placed because I want the place of flag inside the knownUID array(STRING array)
      return 1;
      break;
    }
  }
  return 0;
}
void redLED() { // red LED on, green LED off
  digitalWrite(led_green, LOW);
  digitalWrite(led_red, HIGH);
}
void greenLED() { // red LED off, green LED on
  digitalWrite(led_green, HIGH);
  digitalWrite(led_red, LOW);
  tone(buzzer, 440, 50); // sound; frequency of tone: 440 Hz, duration of tone: 50 ms
}
void error() { // error option
  digitalWrite(led_green, LOW);
  digitalWrite(led_red, LOW);
  delay(150);
  digitalWrite(led_red, HIGH);
  tone(buzzer, 440, 50);
  delay(150);
  digitalWrite(led_red, LOW);
  delay(150);
  digitalWrite(led_red, HIGH);
  tone(buzzer, 440, 50);
  digitalWrite(led_red, LOW);
}
// calculate and store data to SD card
void StoreData(int i, String tag) {
  String fileName=String();
  char file[12];
  int pos;
  
  DateTime time = rtc.now(); // read time from RTC  
  if (LastMonth != time.month()) { // check if there is a new month
    LastMonth = time.month();
    fileName="month" ;
    fileName+= LastMonth;
    fileName+= ".csv";
    fileName.toCharArray(file, sizeof(file));
  }
  else{
    fileName="month" ;
    fileName+= LastMonth;
    fileName+= ".csv";
    fileName.toCharArray(file, sizeof(file));
  }
  if (i>1) pos=(pos-1)/2;
  else pos=pos-1;
  
  if (knownUID[i] == "1") { // departure
    lcd.clear();
    lcd.print("Good Bye");
    Serial.println("he/she is leaving the building");
    departure[0] = time;  // save departure time
    /* calculate working hours and minutes
    int dh = abs(departure[0].hour() - arrival[0].hour());
    int dm = abs(departure[0].minute() - arrival[0].minute());
    unsigned int work = dh * 60 + dm; // working hours in minutes
    MinsA = MinsA + work; // add working hours in minutes to working hours from this month
    HoursA = (int)MinsA / 60; // calculate working hours from minutes
    */
    //enableSd();
    myFile = SD.open(file, FILE_WRITE); // open file with history and write to it
    if (myFile) { // format = " DD-MM-YYYY hh:mm (arrival), hh:mm (departure), hh (working hours today), hh (working hours this month);
      myFile.print(departure[0].year(), DEC);
      myFile.print("-");
      myFile.print(departure[0].month(), DEC);
      myFile.print("-");
      myFile.print(departure[0].day(), DEC);
      myFile.print(",");
      myFile.print(departure[0].hour(), DEC);
      myFile.print(":");
      myFile.print(departure[0].minute(), DEC);
      myFile.print(",");
      myFile.print(tag);
      myFile.print(",");
      myFile.print("departure");
      myFile.println();//change line for next day
      myFile.close();
      delay(150);
    }
    else {
      Serial.println("Reading/Writing file error");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card ERROR");
      lcd.setCursor(0, 1);
      lcd.print("Maintaince need");
      for(;;){};// wait here for ever
    }
    knownUID[i] = "0"; // reset flag
  }
  else { // arrival;
    Serial.println("Someone is get in");
    lcd.clear();
    lcd.print("Welcome");
    arrival[0] = time;  // save time of arrival
    //fileName.toCharArray(file, sizeof(file));
    myFile = SD.open(file, FILE_WRITE); // open file with history and write to it
    if (myFile) { // format = " YYYY-MM-DD hh:mm (arrival), hh:mm (departure), hh (working hours today), hh (working hours this month);
      myFile.print(arrival[0].year(), DEC);
      myFile.print("-");
      myFile.print(arrival[0].month(), DEC);
      myFile.print("-");
      myFile.print(arrival[0].day(), DEC);
      myFile.print(",");
      myFile.print(arrival[0].hour(), DEC);
      myFile.print(":");
      myFile.print(arrival[0].minute(), DEC);
      myFile.print(",");
      myFile.print(tag);
      myFile.print(",");
      myFile.print("arrival");
      myFile.println();//change line for next day
      myFile.close();
      delay(100);
    }
    else {
      Serial.println("Reading/Writing file error");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card ERROR!");
      lcd.setCursor(0, 1);
      lcd.print("Call Service");
      for(;;){};// wait here for ever
    }
    
    knownUID[i] = "1"; // set flag
    delay(100);
  }
}
/*
void webServer() {
  enableEther();
  EthernetClient client = server.available();
  if (client) { // if HTTP request is available
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          //client.println("Refresh: 10");  // refresh the page automatically every 10 sec
          client.println();
          client.println("<!DOCTYPE html>");
          client.println("<html><head><title>Office Atendance Logger</title><style>");
          client.println(".jumbotron{margin: 1% 3% 1% 3%; border: 1px solid none; border-radius: 30px; background-color: #AAAAAA;}");
          client.println(".dataWindow{margin: 1% 3% 1% 3%; border: 1px solid none; border-radius: 30px; background-color: #AAAAAA;padding: 1% 1% 1% 1%;}");
          client.println("</style></head><body style=\"background-color: #E6E6E6\">");
          client.println("<div class=\"jumbotron\"><div style=\"text-align: center\"> <h1>  Office Atendance Logger </h1> </div> ");
          client.println("</div><div class=\"dataWindow\"><div style=\"text-align: center\"> <h2> User A </h2>");
          myFile = SD.open("A.txt");
          if (myFile) {

            while (myFile.available()) {
              client.print("<p>");
              while (DataRead != 59) {
                DataRead = (char)myFile.read();
                client.print(DataRead);
                client.print(myFile.read());
              }
              client.println("</p>");
              DataRead = 0;
            }

            myFile.close();
          }
          client.println("</div></body></html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(10);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
    Ethernet.maintain();
  } // ----------------WEB SERVER END---------------------------
}

*/
