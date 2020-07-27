#include <MKRGSM.h>
#include <SPI.h>
#include <SD.h>

GSM gsmAccess;
GSMVoiceCall vcs;
GSMScanner scannerNetworks;
GSM_SMS sms;

#define ADMINNUMBER "+xxxxxxxxxxx"  // Mobile number that the admin uses to send sms.
#define DOORNUMBER "xxxxxxxxx"      // Mobile number to be called to open the door.
#define PINNUMBER ""                // PIN number of the SIM card of the door.
const int NUMBERSIZE = 20;          // Maximum number length.
int relay = 1;                      // Door relay output.

void setup() {
  Serial.begin(9600);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);
  startSD();
  startGSM();
}

void loop() {

  if (scannerNetworks.getCurrentCarrier().equals("")) {
    Serial.println("GSM not connected.");
    startGSM();
  }
  processVoiceCalls();
  processSMS();
  delay(1000);
}

void startSD() {

  Serial.print("Starting SD card...");

  if (!SD.begin(4)) {
    Serial.println("Fail to start SD card");
    while (1) {}
  }
  Serial.println("SD Card started.");
}

void startGSM () {

  boolean notConnected = true;

  while (notConnected) {
    Serial.println("Starting GSM.");
    if (gsmAccess.begin(PINNUMBER,true)==GSM_READY) {
      notConnected = false;
      Serial.println(scannerNetworks.getCurrentCarrier());
    } else {
      Serial.println("GSM connection failed.");
      delay(1000);
    }
  }
  Serial.println("GSM connected. Waiting for calls.");
  vcs.hangCall();
}

void openDoor() {

  digitalWrite(relay, LOW);
  delay(400);
  digitalWrite(relay, HIGH);
}

void processVoiceCalls() {

  char remoteNumber[NUMBERSIZE];

  switch (vcs.getvoiceCallStatus()) {
    case IDLE_CALL:
      break;
    case CALLING:
      vcs.hangCall();
      break;
    case RECEIVINGCALL:
      Serial.println("Receiving call..");
      vcs.retrieveCallingNumber(remoteNumber, NUMBERSIZE);
      Serial.print("Número: ");
      Serial.println(remoteNumber);
      vcs.hangCall();
      if (isNumberAllowed(remoteNumber)) {
        Serial.println("Number allowed. Door is opening.");
        openDoor();
      } else {
        Serial.println("b");
      }
      break;
    case TALKING:
      vcs.hangCall();
      break;
  }
}

void processSMS() {

  char remoteNumber[NUMBERSIZE];
  char number[NUMBERSIZE];

  if (sms.available()) {
    sms.remoteNumber(remoteNumber,NUMBERSIZE);
    Serial.print("Receiving SMS from: ");
    Serial.println(remoteNumber);
    if (strcmp(remoteNumber, ADMINNUMBER) == 0) {
      int i = 0;
      char action = sms.read();
      int b = 0;
      while (b > -1 && i < NUMBERSIZE) {
        b = sms.read();
        if (b > -1) {
          number[i] = b;
        }
        i++;
      }
      number[i-1] = '\0';
      Serial.print("Message: ");
      Serial.print(action);
      Serial.println(number);
      sms.flush();
      if (action == '1') {                  // if the number sent is preceded by 1 it will be added.
        addNumberToAllowed(number);
      } else if (action == '0' ) {          // if the number sent is preceded by 0 it will be removed.
        removeNumberFromAllowed(number);
      } else if (action == '2') {           // option 2 will send an sms to the admin with a list of allowed numbers.
        getAllowedNumbers();
      } else if (action == '3') {           // option 3 will notify the phone number of the arduino to the allowed numbers.
        notifyAllowedNumbers();
      } else {
        sendSMS(ADMINNUMBER, "Incorrect option.");
      }
    }
  }
}

void addNumberToAllowed(const char * number) {

  if (!isNumberAllowed(number)) {
    File allowedNumbers = SD.open("allowed.txt", FILE_WRITE);
    allowedNumbers.println(number);
    allowedNumbers.close();
    sendSMS(ADMINNUMBER, "Number has been added.");
    notifyAllowedNumber(number);
  } else {
    sendSMS(ADMINNUMBER, "Number already exists.");
  }

}

void removeNumberFromAllowed(const char * number) {

  if (isNumberAllowed(number)) {
    File allowedNumbers = SD.open("allowed.txt", FILE_READ);
    File temp = SD.open("temp.txt", FILE_WRITE);

    while (allowedNumbers.available()) {
      String s = allowedNumbers.readStringUntil('\n');
      char actualNumber[NUMBERSIZE];
      s.toCharArray(actualNumber, s.length());
      if (strcmp(actualNumber, number) != 0) {
        temp.println(actualNumber);
      }
    }
    allowedNumbers.close();
    temp.close();

    SD.remove("allowed.txt");
    allowedNumbers = SD.open("allowed.txt", FILE_WRITE);
    temp = SD.open("temp.txt", FILE_READ);

    while (temp.available()) {
      String s = temp.readStringUntil('\n');
      char actualNumber[NUMBERSIZE];
       s.toCharArray(actualNumber, s.length());
      allowedNumbers.println(actualNumber);
    }
    allowedNumbers.close();
    SD.remove("temp.txt");
    sendSMS(ADMINNUMBER, "Number has been removed.");
  } else {
    sendSMS(ADMINNUMBER, "Number already exists.");
  }
}

boolean isNumberAllowed(const char * number) {

  boolean found = false;
  File allowedNumbers = SD.open("allowed.txt", FILE_READ);

  while (allowedNumbers.available()) {
    String s = allowedNumbers.readStringUntil('\n');
    char actualNumber[NUMBERSIZE];
    s.toCharArray(actualNumber, s.length());
    if (strcmp(actualNumber, number) == 0) {
      found = true;
    }
  }
  allowedNumbers.close();

  return found;
}

void getAllowedNumbers() {

  int size = 0;
  File allowedNumbers = SD.open("allowed.txt", FILE_READ);
  sms.beginSMS(ADMINNUMBER);
  while (allowedNumbers.available()) {
    String s = allowedNumbers.readStringUntil('\n');
    char actualNumber[NUMBERSIZE];
    s.toCharArray(actualNumber, s.length());
    if (size == 0) {
      sms.beginSMS(ADMINNUMBER);
    }
    sms.print(actualNumber);
    sms.print('\n');
    size += s.length();
    if (size > 127) {
      sms.endSMS();
      size = 0;
    }
  }
  sms.endSMS();
  allowedNumbers.close();
}

void sendSMS(const char * number, const char * text) {

  sms.beginSMS(number);
  sms.print(text);
  sms.endSMS();
}

void notifyAllowedNumber(const char * number) {

  sms.beginSMS(number);
  sms.print("Access granted. To open the door call to ");
  sms.print(DOORNUMBER);
  sms.endSMS();
}

void notifyAllowedNumbers() {

  int size = 0;
  File allowedNumbers = SD.open("allowed.txt", FILE_READ);
  sms.beginSMS(ADMINNUMBER);
  while (allowedNumbers.available()) {
    String s = allowedNumbers.readStringUntil('\n');
    char actualNumber[NUMBERSIZE];
    s.toCharArray(actualNumber, s.length());
    notifyAllowedNumber(actualNumber);
  }
  allowedNumbers.close();
}
