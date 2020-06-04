#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); //Create instance of LCD

#define SS_PIN 10  //slave select pin
#define RST_PIN 5  //reset pin
MFRC522 mfrc522(SS_PIN, RST_PIN);        // instatiate a MFRC522 reader object.
MFRC522::MIFARE_Key key;//create a MIFARE_Key struct named 'key', which will hold the card information

float credit=100;////////CREDIT

//declare pins for buzzer and LEDs
int piezo = 5;
int red_light_pin = 6;
int green_light_pin = 7;
int blue_light_pin = 8;


//SETUP
void setup()
{

  Serial.begin(9600);
Serial.setTimeout(30);
  //Initiate RFID Reader
  SPI.begin();               // Init SPI bus
  mfrc522.PCD_Init();        // Init MFRC522 card
  Serial.println("Scanner Ready");
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  delay(200);

  randomSeed(analogRead(0));// seed random function

  //Set PinModes for LEDs and Buzzer
  pinMode(red_light_pin, OUTPUT);
  pinMode(green_light_pin, OUTPUT);
  pinMode(blue_light_pin, OUTPUT);
  pinMode(piezo, OUTPUT);

  //Prepare LCD
  lcd.begin(16, 2);
  lcd.clear();
  //Boot Screen
  lcd.print("WELCOME "); lcd.print((char)188);
  delay(1500);// delay Welcome Text


}


// KWh and tarrif variables
float kwhAvailable = 0;//KWh available
const float kwhTarrif = 0.28;// 0.28 cedis per KWh
const float lowThresh = 36; //minimum Credit Threshold
const float midThresh = 120; //mid Credit Threshold
float kwhUsed;// Total used KWh
bool credCrit = false;// boolean to check if credit is below threshold
float kw = 0;// periodic used KWh


int block = 2; //this is the block number we will read from.
byte buffer[18] = {0}; //This array is used for reading out a block.
byte size = sizeof(buffer);//variable with the read buffer size
float cardVal = 0; // value read from card


//LOOOOP
void loop()
{
  size = sizeof(buffer);

  //check Credit threshold : low/ medium/ high
  credAlert();

  // Print Info On LCD
  detectCard();
  dispScreen1();
  dispScreen2();

  //Use Credit
  if (!credCrit) {
    useCredit();
  }
 
  detectCard();
  credAlert();

  if (Serial.available() > 0) {
    float x = Serial.parseFloat();
    Serial.println("");
    Serial.print("Number received: "); Serial.println(x);
    credit += x;//Update Credit with recharge value
  }

}

void useCredit()
{
  kw  = random(0, 50);// generate random KWh value
  credit -= (kwhTarrif * kw);
  kwhUsed += kw;
  kwhAvailable = credit / kwhTarrif;
  Serial.print("KWh:"); Serial.print(kw);
  Serial.println();
  delay(200);
}

void credAlert() {
  if (credit < lowThresh * kwhTarrif) {// Low Credit Threshold
    RGB_color(255, 0, 0); // Red
    credCrit = true;
    tone(piezo, 1000);
    dispLowCred();

  } else if (credit >= lowThresh * kwhTarrif && credit <= midThresh * kwhTarrif) {//medium Credit Threshold
    noTone(piezo);
    RGB_color(255, 255, 0); // Yellow
    credCrit = false;
  } else {// High Credit Threshold
    noTone(piezo);
    RGB_color(0, 255, 0); // Green
    credCrit = false;
  }

}


void dispScreen1() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Used KWh:"); lcd.print(kwhUsed);
  lcd.setCursor(0, 1);
  lcd.print("Balance:"); lcd.print((char)236); lcd.print(credit);
  delay(2500);
}


void dispScreen2() {
  kwhAvailable = credit / kwhTarrif;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Available KWh:");
  lcd.setCursor(5, 1);
  lcd.print(kwhAvailable);
  delay(2500);
}

void dispLowCred() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("CAUTION"); lcd.print((char)33);
  lcd.setCursor(0, 1);
  lcd.print("Credit Very Low");
  delay(1500);
}

int readBlock(int blockNumber)
{

  int largestModulo4Number = blockNumber / 4 * 4;
  int trailerBlock = largestModulo4Number + 3; //determine trailer block for the sector

  /*****************************************authentication of the desired block for access***********************************************************/
  byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print("PCD_Authenticate() failed (read): ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return 3;//return "3" as error message
  }
  /*****************************************reading a block***********************************************************/
  status = mfrc522.MIFARE_Read(blockNumber, buffer, &size);
  \
  if (status != MFRC522::STATUS_OK) {
    Serial.print("MIFARE_read() failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));

    return 4;//return "4" as error message
  }
  Serial.println("Recharge Successful");
}
void RGB_color(int red_light_value, int green_light_value, int blue_light_value)
{
  analogWrite(red_light_pin, red_light_value);
  analogWrite(green_light_pin, green_light_value);
  analogWrite(blue_light_pin, blue_light_value);
}

void detectCard() {
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println("Card Detected");
  readBlock(block);//read the block back

  String readStr = "";
  Serial.println();
  Serial.println( "---------- LOADING CREDIT ----------:");
  Serial.print("Card Value: ");
  for (int j = 0 ; j < 16 ; j++) //get the block contents
  {
    char x = buffer[j];
    readStr.concat(x);
  }
  cardVal = readStr.toFloat();
  credit += cardVal;

  Serial.println( readStr);
  Serial.print( "Old Balance:"); lcd.print((char)236); Serial.print( credit);
  Serial.println();
  Serial.print( "New Balance:"); lcd.print((char)236); Serial.print( credit);
  Serial.println();
// show Recharge on LCD
  lcd.clear();
  lcd.print( "CREDIT RECHARGE");
  lcd.setCursor(0, 1);
  lcd.print("Card Value: "); lcd.print((char)236); lcd.print(cardVal);
  delay(1500);
  lcd.clear();
  lcd.print("KWh Paid:"); lcd.print(cardVal / kwhTarrif);
  lcd.setCursor(0, 1);
  lcd.print("New Bal:"); lcd.print(credit);
  tone(piezo, 1200);
  delay(400);
  noTone(piezo);
  delay(2000);
  mfrc522.PCD_Reset(); mfrc522.PCD_Init();//Reset RFID reader from next recharge

}
