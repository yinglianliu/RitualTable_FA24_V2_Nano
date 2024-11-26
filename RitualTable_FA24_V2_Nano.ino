/*
  Ritual Table by Yinglian Liu 
  Fall 2024
    -This is a project for the final puzzle of the Escape Room Project of ENT department.
    -Using 5 RFID Readers to detect 5 target items，if all the items are collected and placed in the correct spot, 
     the candle bar with 5 candles will be lit and puzzle will be solved. 
    -Each item corresponds to a candle. When the item is placed in the correct spot, the corresponding candle will be lit. 
     If it is one of the 5 target items but placed in the wrong spot, all candles will flash quickly a few times(as a visual cue). 
     If it is not one of the 5 target objects, there will be no prompt.
*/

/*
v7: 11/26 
Update the rules for candles being lit or turn off: 
If the item and reader match, the corresponding candle is lit. 
If the candle is already lit but the audience moves the object away, turn off the corresponding candle. 
If the audience puts the item in an mismatched reader position, flash three times and keep the correctly lit candle lit.
 */

/* 
v6: Add a 8 channel relay module to control light, cut the mp3 player, maglock
    Updating the compareUID function in order to compare difference tags with different UID size.
    (Ex:Some of the sticker with 7 bytes UID size and the white cards and blue tags with 4 bytes UID size)
v5: Add a Serial mp3 player(GD3300D chip) to play sound effect
    Add a relay to control a mag lock
v4: Add reset function. Add one more card as a reset card to reset the program to initail state.
*/

/* 
-------Wiring-------- 

*/

//Set a slower SPI clock speed to prevent data loss if the wires are getting longer
//#define MFRC522_SPICLOCK (400000u) // 400kHz

#include <SPI.h>
#include <MFRC522.h> // RFID library
//#include <SoftwareSerial.h> // For MP3 module

// RFID reader number
const uint8_t Num_Readers = 5;
// Each RFID reader SS(SDA) pins

//For Mega
//const uint8_t ssPins[Num_Readers] = {38,36,34,32,30}; //The pin order must match the order of the targetUIDs array
//const uint8_t rstPin = 22; // All readers share the common rst pin 

//For Nano 33 IOT
const uint8_t ssPins[Num_Readers] = {9,8,7,6,5}; //The pin order must match the order of the targetUIDs array
const uint8_t rstPin = 10; // All readers share the common rst pin 

MFRC522 rfid[Num_Readers];

// Target UIDs for each reader（for 4 bytes UID, add 0x00 to 7 bytes)
byte targetUIDs[Num_Readers][7] = {
  {0x04, 0x34, 0xAB, 0xC5, 0x79, 0x00, 0x00},  // Reader 0 (7byte) Trophies
  {0x81, 0x46, 0x21, 0x03, 0x00, 0x00, 0x00},  // Reader 1 (4byte) Sheet Music
  {0x04, 0x36, 0xAB, 0xC5, 0x79, 0x00, 0x00},  // Reader 2 (7byte) Amulet
  {0xA1, 0xFD, 0x27, 0x03, 0x00, 0x00, 0x00},  // Reader 3 (4byte) Journal
  {0x04, 0xAA, 0xAA, 0xC5, 0x79, 0x00, 0x00},  // Reader 4 (7byte) Ring box
 
};

// The Actual UID size for each target UID
byte targetUIDLengths[Num_Readers] = {7, 4, 7, 4, 7};

//Set up the reset card (keep the same format of the target UID)
byte resetUID[7] = {0xF3, 0x9A, 0x2B, 0xAB, 0x00, 0x00, 0x00};
const byte resetUIDLength = 4;


// Array to track if the correct card has been detected for each reader
bool cardDetected[] = {false, false, false, false, false};

//The pin order in relay modules：      IN8,IN7,IN6,IN5,IN4
const uint8_t relayPins[Num_Readers] = {A0, A1, A2, A3, A4};

bool puzzleSolved = false;

//use this array to track which candles has been lighted
bool lightedCandles[Num_Readers] = {false, false, false, false, false};

void setup() {
  Serial.begin(115200);
  //delay(1000);
  //while (!Serial);

  // Initialize the relay pins as outputs
  for (uint8_t i = 0; i < Num_Readers; i++) {
    pinMode(relayPins[i], OUTPUT);
    // Start with all relays off
    digitalWrite(relayPins[i], LOW);
  }

  //Initialize different LED pins as output and off
  // for (uint8_t i = 0; i < Num_Readers; i++) {
  //   pinMode(ledPins[i], OUTPUT); 
  //   digitalWrite(ledPins[i], LOW); 
  // }
    // pinMode(ledResetPin, OUTPUT); 
    // digitalWrite(ledResetPin, LOW); 

  //Initialize the SDA pin aka SS pin as output mode and set the state as inactive (HIGH as inactive, LOW as active)
  for (uint8_t i=0; i < Num_Readers; i++) {
		pinMode(ssPins[i], OUTPUT);
		digitalWrite(ssPins[i], HIGH);
	}

  SPI.begin(); // Initialize SPI bus

  // Initialize RFID readers ( References: library: MFRC522 ->DumpInfo )
  for (uint8_t reader = 0; reader < Num_Readers; reader++) {
    rfid[reader].PCD_Init(ssPins[reader],rstPin);
    delay(4);
    Serial.print("RFID reader ");
    Serial.print(reader);
    Serial.println(" initialized");
    rfid[reader].PCD_DumpVersionToSerial(); /*Add this to be clear if all the readers are communicated to the microcontroller successfully
    (If it cann't be printed the version infor into the serial monitor, it means the reader cannot comunicate with the microcontroller)*/
  }

}

void loop() {

  bool allCorrectCardsDetected = true;

  for (uint8_t reader = 0; reader < Num_Readers; reader++) {
    // Deactivate all readers
    for (uint8_t i = 0; i < Num_Readers; i++) {
      digitalWrite(ssPins[i], HIGH);
    }
    // Activate the current reader
    digitalWrite(ssPins[reader], LOW);

    // Small delay to allow the SPI bus to stabilize
    delay(10);

    // Check for new cards and read UID
    if (rfid[reader].PICC_IsNewCardPresent() && rfid[reader].PICC_ReadCardSerial()) {
      // Read UID
      byte* uid = rfid[reader].uid.uidByte;
      byte uidSize = rfid[reader].uid.size;

      // Print detected UID
      Serial.print("Reader ");
      Serial.print(reader);
      Serial.print(" detected UID: ");
      for (byte i = 0; i < uidSize; i++) {
        Serial.print(uid[i] < 0x10 ? " 0" : " ");
        Serial.print(uid[i], HEX);
      }
        Serial.println();

      // Compare detected UID with target UID
      if (compareUID(uid, uidSize, targetUIDs[reader], targetUIDLengths[reader])) {
        Serial.print("Reader ");
        Serial.print(reader);
        Serial.println(": Target item detected!");

        cardDetected[reader] = true;
        //digitalWrite(ledPins[reader], HIGH);

        if (!lightedCandles[reader]) {
          digitalWrite(relayPins[reader], HIGH);
          lightedCandles[reader] = true;
        }

      } else if(compareUID(uid, uidSize, resetUID, resetUIDLength)) {
        Serial.println("Reset card detected! Reseting...");
        //Run reset function
        resetToInitialState();

      } else {
        Serial.print("Reader ");
        Serial.print(reader);
        Serial.println(": UID does not match."); 
          // Flash all LEDs
        flashAllCandles(reader, 3, 100); // Flash all candles 3 times and turn off the current candle
      }
      //Stop and clear the communication
      rfid[reader].PICC_HaltA(); 
      rfid[reader].PCD_StopCrypto1();
    } //Check for new cards end here

    /* when detected item is moved,
    if PICC_IsNewCardPresent() == false, no tag is close or contact,
    and if lightedCandles is true, it means the candle is lit before. 
    So, this two conditions can be used to detecte if the corrted item has been moved
    */
    if(!rfid[reader].PICC_IsNewCardPresent() && lightedCandles[reader]){

      Serial.print("Candle for reader ");
      Serial.print(reader);
      Serial.println(" turned off as item moved.");

      lightedCandles[reader] = false;
      cardDetected[reader] = false;
      digitalWrite(relayPins[reader], LOW);
    }

    // Deactivate the current reader
    digitalWrite(ssPins[reader], HIGH);

    //If not all the reader detected the corrsponding tag, change the "allCorrectCardsDetected" to false
    if (!cardDetected[reader]) {
      allCorrectCardsDetected = false;
    }
    
    // Keep the LED on if the correct card was previously detected
    if (cardDetected[reader]) {
      //digitalWrite(ledPins[reader], HIGH);
      digitalWrite(relayPins[reader], HIGH);
    } else {
      //digitalWrite(ledPins[reader], LOW);
      digitalWrite(relayPins[reader], LOW);
    }
  }

  if (allCorrectCardsDetected && !puzzleSolved ) {
    Serial.println("All correct tags detected!");
    delay(2000);

    //repeat the relay animation 3 times
    for(uint8_t repeat = 0; repeat < 3; repeat++){
      //start from the first relay
      for(uint8_t i = 0; i < Num_Readers; i++){

        digitalWrite(relayPins[i], HIGH);
        delay(50);
        digitalWrite(relayPins[i], LOW);
        delay(50);
      }
      //start from the last relay
      for (int i = Num_Readers - 1; i >=0; i--){
        digitalWrite(relayPins[i], HIGH);
        delay(50);
        digitalWrite(relayPins[i], LOW);
        delay(50);
      }

    }
    puzzleSolved = true;
  } 

}


//////////////////////////////////////////////Defind Functions///////////////////////////////////////////////////////////////////////////////////

/* -----------------Compare two UIDs-----------------------*/
// Function to compare UIDs, returns false if UID lengths don't match
bool compareUID(byte* uid1, byte uid1Size, byte* uid2, byte uid2Size) {
  // if UID lengths don't match，returns false
  if (uid1Size != uid2Size) {
    return false;
  }
  // compare each UIDs
  for (byte i = 0; i < uid1Size; i++) {
    if (uid1[i] != uid2[i]) {
      return false;
    }
  }
  return true; // all UID match
}


/* ------------------Flash all Candles---------------------*/
void flashAllCandles(uint8_t times, unsigned long duration) {
  for (uint8_t t = 0; t < times; t++) {
    for (uint8_t i = 0; i < Num_Readers; i++) {
      //digitalWrite(ledPins[i], HIGH);
      digitalWrite(relayPins[i],HIGH);
    }
    delay(duration);
    for (uint8_t i = 0; i < Num_Readers; i++) {
      //digitalWrite(ledPins[i], LOW);
      digitalWrite(relayPins[i], LOW);
    }
    delay(duration);
  }

  //Turn on the corrected candles back
  for(uint8_t i=0; i < Num_Readers; i++) {

    if(lightedCandles[i]) {

      digitalWrite(relayPins[i], HIGH);
    }
  }
}

/* ------------------Flash and reset Candles---------------------*/
//use to turn off the candle when candle has been turn on but move to other mismatch reader
// void flashAndResetCandle(uint8_t reader, uint8_t times, unsigned long duration) {
//     flashAllCandles(times, duration); 

//     // turn off the current candle
//     lightedCandles[reader] = false;
//     cardDetected[reader] = false;
//     digitalWrite(relayPins[reader], LOW);
// }

/* ------------------Reset to Initial State---------------------*/
void resetToInitialState() { 
  // reset all detected tags and cards to false
  for (uint8_t i = 0; i < Num_Readers; i++) {
    cardDetected[i] = false;
    lightedCandles[i] = false;
    digitalWrite(relayPins[i], LOW);

  }
  
    //allCorrectCardsDetected = false;
    puzzleSolved = false;
    delay(1000);
    Serial.println("Program has been reset...");

}