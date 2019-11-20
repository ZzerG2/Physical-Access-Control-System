#include <SPI.h>
// RFID Library (using package manager). v1.4.5
#include <MFRC522.h> 
#include <EEPROM.h>
#define SS_PIN 10
#define RST_PIN 9                                                                                                                                            
#define PIN_RELAY 5
#define GREENLED 2
#define REDLED 4
#define VOLTAGE_PIN A1
#define BUTTON_RELAY 7

unsigned long MasterCard = 3507708549; // todo: to EEPROM
unsigned long OffCard = 0;
unsigned long g_ctr = 0;
unsigned long c_ctr = 0;
byte ee_cards = 0;
bool new_state_event = false;
#define MAX_CARDS 10
#define EMPTY_CARD 0xffffffff
#define CARDS_CTR_ADDRESS 0
#define FIRST_CARD_ADDRESS 1



MFRC522 mfrc522(SS_PIN, RST_PIN);

enum GlobalStates {
  gsSimpleMode,
  gsMasterMode
};

GlobalStates globalState = gsSimpleMode;

enum ReadCardStates {
  rcState_Init,
  rcState_Present,
  rcState_Read,
  rcState_Ready,  
  rcState_Ready2  
};

ReadCardStates rcState = rcState_Init;

enum RelayStates {
  relState_ON,
  relState_OFF
};

RelayStates relState = relState_OFF;

enum ButtonrelStates {
  ButtonrelState_ON,
  ButtonrelState_OFF
};
ButtonrelStates ButtonrelState = ButtonrelState_OFF;



unsigned long CurrentCard = 0;
  
void setup() {
  pinMode(GREENLED, OUTPUT);
  pinMode(REDLED, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(BUTTON_RELAY, OUTPUT);
  Serial.begin(115200);
  digitalWrite(GREENLED, HIGH);
  digitalWrite(REDLED, HIGH);
  delay(2000);
  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, LOW);
  digitalWrite(PIN_RELAY, HIGH);
  digitalWrite(BUTTON_RELAY, HIGH);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  SPI.begin();
  mfrc522.PCD_Init();  
  ee_cards = EEPROM.read(CARDS_CTR_ADDRESS);
  if (ee_cards == 0xff) {
    ee_cards = 0;
    update_ee_cards();
  }
  Serial.print("EE_CARDS: ");
  Serial.print(ee_cards);
  Serial.print(" MAX_CARDS: ");
  Serial.print(MAX_CARDS);
  Serial.println("");
  Serial.println("*** Time to Roll out ***");
  dump_eeprom();
}

// EEPROM PROCESSING BEGIN ///////////////////////////////////////////////////////////
void update_ee_cards()
{
  EEPROM.update(CARDS_CTR_ADDRESS, ee_cards);
}

int check_ee_card(unsigned long checked_card)
{
  for (int i = 0; i < ee_cards; i++ ) {
    unsigned long card = read_card(i);
    if (card == EMPTY_CARD) {
      continue;  
    }  
    if (card == checked_card) {
      return i;  
    }
  }
  return -1;
}

/**
 * Card Num: 0..
 */
unsigned long read_card(int card_num)
{
  int address = card_num * sizeof(unsigned long) + FIRST_CARD_ADDRESS;
  unsigned long res = 0;
  for (int i = 0; i < sizeof (unsigned long); i++) {
    byte value = EEPROM.read(address + i);
    res = res * 256 + value;  
  }
  return res;
}

void write_card_by_num(int card_num, unsigned long card_value)
{
  int address = card_num * sizeof(unsigned long) + FIRST_CARD_ADDRESS;
  unsigned long res = 0;
  for (int i = sizeof (unsigned long)-1; i >=0; i--) {
    byte value = card_value % 256;
    card_value /= 256;
    EEPROM.update(address + i, value);
  }
}

void delete_card(int card_num, unsigned long card_value)
{
  write_card_by_num(card_num, EMPTY_CARD);
}

int write_card(unsigned long card_value)
{
  int i;
  for (i = 0; i < ee_cards; i++ ) {
    unsigned long card = read_card(i);
    if (card == EMPTY_CARD) { // empty place -> reuse
      digitalWrite(GREENLED, LOW);
      delay(1000);
      digitalWrite(GREENLED, HIGH);
      Serial.print("Reuse place ");
      Serial.print(i);
      Serial.println();
      write_card_by_num(i, card_value);
      return i;  
    }  
  }
  if (ee_cards == MAX_CARDS) {
    Serial.println("Too many cards!!!");
    return -1;
  }
  ee_cards += 1;
  update_ee_cards();
  i = ee_cards-1;
  write_card_by_num(i, card_value);
  Serial.print("New place ");
  Serial.print(i);
  Serial.println();
  return i;
}

void dump_eeprom()
{
  for (int address = 0; address < EEPROM.length(); address++) {
    // read a byte from the current address of the EEPROM
    byte value = EEPROM.read(address);
    if (address % 16 == 0) {
      Serial.println();
      Serial.print(address, HEX);
      Serial.print("\t");
    }
    Serial.print(value, HEX);
    if (address && (address+1) % 8 == 0 && address % 16 != 0) {
      Serial.print("  ");
    }
    else {
      Serial.print(" ");
    }
    if (address % 16 == 0) {
      // Serial.println();
    }
  }
}

// EEPROM PROCESSING END   ///////////////////////////////////////////////////////////

unsigned long read_card()
{
    // read RFID
    unsigned long uidDec = 0;
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        byte uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec * 256 + uidDecTemp;
    }
    return uidDec;
}

bool change_state(ReadCardStates new_state, const char * info)
{
  if (new_state != rcState) {
    Serial.print(g_ctr);
    Serial.print(" :: ");
    Serial.print(rcState);   Serial.print(" -> "); Serial.print(new_state); Serial.print(":");
    Serial.println(info);
    rcState = new_state;
    g_ctr = 0;
    c_ctr = 0;
    CurrentCard = 0;
    new_state_event = true;
    return true;
  }  
  new_state_event = false;
  return false;
}

/**
 * Check Card. Master Mode; 
 */
void check_card_2()
{
  if ( !mfrc522.PICC_IsNewCardPresent()) {
      if (rcState != rcState_Present && g_ctr < 3) { // bounce???
        return;
      }
      change_state(rcState_Present, "IsNewCardPresent=0");
      return;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
      if (rcState != rcState_Read && g_ctr < 2) { // bounce???
        return;
      }
      change_state(rcState_Read, "ReadCardSerial=0");
      return;
  }
  unsigned long new_card = read_card();
  if (rcState == rcState_Ready2 && new_card == CurrentCard) {
    c_ctr = 0;
    g_ctr = 0; // Reset counter;
    return;
  }
  if ( change_state(rcState_Ready, "Card!") ) {
    CurrentCard = new_card;
    c_ctr = 0;
    if (new_card == MasterCard) {
      Serial.print("To Simple Mode");
      digitalWrite(GREENLED, LOW);
      globalState = gsSimpleMode;
      return;
    }
    int num = check_ee_card(new_card);
    if (num < 0) {
      Serial.print("Found new card ");
    }
    else
    {
      Serial.print("Found card[");
      Serial.print(num);
      Serial.print("]");
    }
    Serial.println(new_card);
  }
  else {
    c_ctr += 1;
    g_ctr = 0; // Reset counter;
    // todo: check card number???  
  }
  if (rcState == rcState_Ready && c_ctr > 20) {
    if (new_card == MasterCard) {
      change_state(rcState_Ready, "Master Card(Long Tap)"); 
      CurrentCard = new_card;
      Serial.print("To Simple Mode");
      globalState = gsSimpleMode;
      return;
    }
    change_state(rcState_Ready2, "Card(Long Tap)"); 
    CurrentCard = new_card;
    int num = check_ee_card(new_card);
    if (num < 0) {
      Serial.print("Write Card ");
      Serial.println(new_card);
      write_card(new_card);
      dump_eeprom();
    }
    else {
      digitalWrite(GREENLED, LOW);
      digitalWrite(REDLED, HIGH);
      delay(1000);
      digitalWrite(REDLED, LOW);
      digitalWrite(GREENLED, HIGH);
      Serial.print("Delete Card ");
      Serial.print(new_card);
      Serial.print(" at ");
      Serial.println(num);
      delete_card(num, new_card);
      dump_eeprom();
    }
  }
}


/**
 * Check Card. Simple Mode; 
 */
void check_card()
{
  if ( !mfrc522.PICC_IsNewCardPresent()) {
      if (rcState != rcState_Present && g_ctr < 3) { // bounce???
        return;
      }
      change_state(rcState_Present, "IsNewCardPresent=0");
      return;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
      if (rcState != rcState_Read && g_ctr < 2) { // bounce???
        return;
      }
      change_state(rcState_Read, "ReadCardSerial=0");
      return;
  }
  unsigned long new_card = read_card();
  if ( change_state(rcState_Ready, "Card!") ) {
    CurrentCard = new_card;
    c_ctr = 0;
    if (new_card == MasterCard) {
      if (relState == relState_ON) { // block master mode
        Serial.print("Ignore!");
        blink_red();
        return;
      }
      else {
        Serial.print("To Master Card Mode");
        digitalWrite(GREENLED,HIGH);
        globalState = gsMasterMode;
        return;
      }
    }
    int num = check_ee_card(new_card);
    if (num < 0) {
      Serial.print("Bad card ");
      Serial.println(new_card);
      // todo: bad card processing
      digitalWrite(REDLED, HIGH);
      delay(200);
      digitalWrite(REDLED, LOW);
      return;
    }
    else
    {
      Serial.print("Good card[");
      Serial.print(num);
      Serial.print("]");
      Serial.println(new_card);
      // todo: good card processing
      good_processing();
      //button_comp();
    }
  }
  else {
    c_ctr += 1;
    g_ctr = 0; // Reset counter;
    // todo: check card number???  
  }
}

//Реле включающая диски
void good_processing()
{
  switch (relState) {
    case relState_ON:
      Turn_ON();
      break;  
    case relState_OFF:
      Turn_OFF();
      break;
  }
}

void blink_green()
{
  digitalWrite(GREENLED,HIGH);
  delay(250);
  digitalWrite(GREENLED,LOW);
}

void blink_red()
{
  digitalWrite(REDLED,HIGH);
  delay(250);
  digitalWrite(REDLED,LOW);
}

void Turn_OFF()
{
  OffCard = CurrentCard;
  relState = relState_ON;
  Serial.print("State OFF->ON");
  digitalWrite(PIN_RELAY, LOW);
  blink_green();
  return;
}

void Turn_ON()
{
  if (CurrentCard != OffCard){
    Serial.print("State ON");
    blink_red();
  }else{
    OffCard = 0;
    relState = relState_OFF;
    Serial.print("State ON->OFF");
    blink_green();
  }
  return;
}
//Выключение реле при выключении ПК
void computer_off()
{
    if (analogRead(VOLTAGE_PIN) <= 500){
      delay(5000);
      digitalWrite(PIN_RELAY, HIGH);
    }
}
//void computer_off2()
//{
//    if (analogRead(VOLTAGE_PIN) <= 500){
//      //`delay(5000);
//      digitalWrite(BUTTON_RELAY, HIGH);
//    }
//}
//Работа кнопки включения ПК только после поднесения карты
//void button_comp()
//{
//  switch (ButtonrelState) {
//    case ButtonrelState_ON:
//      Button_ON();
//      break;  
//    case ButtonrelState_OFF:
//      Button_OFF();
//      break;
//  }
//}
//void Button_OFF()
//{
//  OffCard = CurrentCard;
//  ButtonrelState = ButtonrelState_ON;
//  Serial.print("State Button OFF->ON");
//  digitalWrite(BUTTON_RELAY, LOW);
//  //blink_green();
//  return;
//}
//
//void Button_ON()
//{
//  if (CurrentCard != OffCard){
//    Serial.print("State Button ON");
//    //blink_red();
//  }else{
//    OffCard = 0;
//    ButtonrelState = ButtonrelState_OFF;
//    Serial.print("State ON->OFF");
//    //blink_green();
//  }
//  return;
//}



void loop() {
  g_ctr += 1;
  // put your main code here, to run repeatedly:
  computer_off();
  //computer_off2();
  switch (globalState) {
    case gsSimpleMode:
      check_card();
      break;  
    case gsMasterMode:
      check_card_2();
      break;  
    default:
      Serial.print("Unknown global state ");
      Serial.println(globalState);
      break;
  }
  delay(50);
}
