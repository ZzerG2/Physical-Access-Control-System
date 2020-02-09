#include <SPI.h>
// RFID Library (using package manager). v1.4.5
#include <MFRC522.h> 
#include <EEPROM.h>

//#define USE_SERIAL_DEBUG

#define GREENLED 2
#define REDLED 4
#define VOLTAGE_PIN A1
// #define PIN_RELAY 5
//
#define RELAY_OFF LOW
#define RELAY_ON HIGH
//
#define BUTTON_RELAY_OFF LOW
#define BUTTON_RELAY_ON HIGH

#define BUTTON_RELAY 8

unsigned long MasterCard = 3507708549; // todo: to EEPROM
unsigned long OffCard = 0;
unsigned long g_ctr = 0;
unsigned long c_ctr = 0;
byte ee_cards = 0;
bool new_state_event = false;
#define MAX_CARDS 5
#define MAX_RELAY_PINS 5
#define EMPTY_CARD 0xffffffff
#define CARDS_CTR_ADDRESS 0
#define FIRST_CARD_ADDRESS 1
//                                 0 1 2 3 4 
int relay_pins [MAX_RELAY_PINS] = {7,6,5,3,1};


#define SS_PIN 10
#define RST_PIN 9
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

enum PowerStates {
  powerState_OFF,
  powerState_ON
};

PowerStates powerState = powerState_OFF;

unsigned long CurrentCard = 0;

void soft_reset()
{
  #ifdef USE_SERIAL_DEBUG
    Serial.println("Soft Reset Start");
  #endif
  globalState = gsSimpleMode;
  rcState = rcState_Init;
  relState = relState_OFF;
  powerState = powerState_OFF;
  CurrentCard = 0;
  OffCard = 0;
  g_ctr = 0;
  c_ctr = 0;
  new_state_event = false;
  #ifdef USE_SERIAL_DEBUG
    Serial.println("Soft Reset Done");
  #endif
}
  
void setup() {
  pinMode(GREENLED, OUTPUT);
  pinMode(REDLED, OUTPUT);
  for (int i = 0; i < MAX_RELAY_PINS; i++) {
    pinMode(relay_pins[i], OUTPUT);
  }
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(BUTTON_RELAY, OUTPUT);
  #ifdef USE_SERIAL_DEBUG
    Serial.begin(115200);
  #endif
  digitalWrite(GREENLED, HIGH);
  digitalWrite(REDLED, HIGH);
  delay(1000);
  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, LOW);
  for (int i = 0; i < MAX_RELAY_PINS; i++) {
    digitalWrite(relay_pins[i], RELAY_OFF);
  }
  digitalWrite(BUTTON_RELAY, BUTTON_RELAY_OFF);
  #ifdef USE_SERIAL_DEBUG
    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }
  #endif
  SPI.begin();
  mfrc522.PCD_Init();  
  ee_cards = EEPROM.read(CARDS_CTR_ADDRESS);
  if (ee_cards == 0xff) {
    ee_cards = 0;
    update_ee_cards();
  }
  #ifdef USE_SERIAL_DEBUG
    Serial.print("EE_CARDS: ");
    Serial.print(ee_cards);
    Serial.print(" MAX_CARDS: ");
    Serial.print(MAX_CARDS);
    Serial.println("");
    Serial.println("*** Time to Roll out ***");
  #endif
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

void reset_eeprom()
{
  for (int i = 0; i < MAX_CARDS; i++) {
    write_card_by_num(i, EMPTY_CARD);
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
      #ifdef USE_SERIAL_DEBUG
        Serial.print("Reuse place ");
        Serial.print(i);
        Serial.println();
      #endif      
      write_card_by_num(i, card_value);
      return i;  
    }  
  }
  if (ee_cards == MAX_CARDS) {
    #ifdef USE_SERIAL_DEBUG
      Serial.println("Too many cards!!!");
    #endif
    return -1;
  }
  ee_cards += 1;
  update_ee_cards();
  i = ee_cards-1;
  write_card_by_num(i, card_value);
  #ifdef USE_SERIAL_DEBUG
    Serial.print("New place ");
    Serial.print(i);
    Serial.println();
  #endif
  return i;
}

void dump_eeprom()
{
  #ifdef USE_SERIAL_DEBUG
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
  #endif
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
    #ifdef USE_SERIAL_DEBUG
      Serial.print(g_ctr);
      Serial.print(" :: ");
      Serial.print(rcState);   Serial.print(" -> "); Serial.print(new_state); Serial.print(":");
      Serial.println(info);
    #endif    
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
void perform_master_mode()
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
      #ifdef USE_SERIAL_DEBUG
        Serial.print("To Simple Mode");
      #endif
      digitalWrite(GREENLED, LOW);
      globalState = gsSimpleMode;
      return;
    }
    int num = check_ee_card(new_card);
    #ifdef USE_SERIAL_DEBUG
      if (num < 0) {
        Serial.print("New card ");
      }
      else
      {
        Serial.print("Found card[");
        Serial.print(num);
        Serial.print("]");
      }
      Serial.println(new_card);
    #endif    
  }
  else {
    c_ctr += 1;
    g_ctr = 0; // Reset counter;
    // todo: check card number???  
  }
  if (rcState == rcState_Ready && c_ctr > 20) {
    if (new_card == MasterCard) {
      change_state(rcState_Ready, "MCard(L-Tap)"); 
      CurrentCard = new_card;
      #ifdef USE_SERIAL_DEBUG
        Serial.print("To Simple Mode");
      #endif
      globalState = gsSimpleMode;
      return;
    }
    change_state(rcState_Ready2, "Card(L-Tap)"); 
    CurrentCard = new_card;
    int num = check_ee_card(new_card);
    if (num < 0) {
      #ifdef USE_SERIAL_DEBUG
        Serial.print("Write Card ");
        Serial.println(new_card);
      #endif
      write_card(new_card);
      dump_eeprom();
    }
    else {
      digitalWrite(GREENLED, LOW);
      digitalWrite(REDLED, HIGH);
      delay(1000);
      digitalWrite(REDLED, LOW);
      digitalWrite(GREENLED, HIGH);
      #ifdef USE_SERIAL_DEBUG
        Serial.print("Delete Card ");
        Serial.print(new_card);
        Serial.print(" at ");
        Serial.println(num);
      #endif
      delete_card(num, new_card);
      dump_eeprom();
    }
  }
}


/**
 * Check Card. Simple Mode; 
 */
void perform_simple_mode()
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
        #ifdef USE_SERIAL_DEBUG
          Serial.print("Ignore!");
        #endif
        blink_red();
        return;
      }
      else {
        #ifdef USE_SERIAL_DEBUG
          Serial.print("To Master Card Mode");
        #endif
        reset_eeprom();
        digitalWrite(GREENLED,HIGH);
        globalState = gsMasterMode;
        return;
      }
    }
    int num = check_ee_card(new_card);
    if (num < 0) {
      #ifdef USE_SERIAL_DEBUG
        Serial.print("Bad card ");
        Serial.println(new_card);
      #endif
      // todo: bad card processing
      digitalWrite(REDLED, HIGH);
      delay(200);
      digitalWrite(REDLED, LOW);
      return;
    }
    else
    {
      #ifdef USE_SERIAL_DEBUG
        Serial.print("Good card[");
        Serial.print(num);
        Serial.print("]");
        Serial.println(new_card);
      #endif
      // todo: good card processing
      good_processing(num);
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
void good_processing(int card_num)
{
  switch (relState) {
    case relState_ON:
      //Turn_ON();
      blink_red();
      blink_red();
      break;  
    case relState_OFF:
      Turn_OFF(card_num);
      break;
  }
}

#define SHORT_BLINK_DELAY 300

void blink_green()
{
  digitalWrite(GREENLED,HIGH);
  delay(SHORT_BLINK_DELAY);
  digitalWrite(GREENLED,LOW);
  delay(SHORT_BLINK_DELAY);
}

void blink_green_red()
{
  digitalWrite(GREENLED,HIGH);
  digitalWrite(REDLED,HIGH);
  delay(SHORT_BLINK_DELAY);
  digitalWrite(GREENLED,LOW);
  digitalWrite(REDLED,LOW);
  delay(SHORT_BLINK_DELAY);
}

void blink_red()
{
  digitalWrite(REDLED,HIGH);
  delay(SHORT_BLINK_DELAY);
  digitalWrite(REDLED,LOW);
  delay(SHORT_BLINK_DELAY);
}

void Power_OFF()
{
  #ifdef USE_SERIAL_DEBUG
    Serial.println("Power Off Mode Begin");
  #endif
  digitalWrite(REDLED,HIGH);
  delay(5000);
  for (int i = 0; i < MAX_RELAY_PINS; i++) {
    pinMode(relay_pins[i], OUTPUT);
    digitalWrite(relay_pins[i], RELAY_OFF);
  }
  digitalWrite(BUTTON_RELAY, BUTTON_RELAY_OFF);
  digitalWrite(REDLED,LOW);
  soft_reset();
  #ifdef USE_SERIAL_DEBUG
    Serial.println("Power Off Mode End");
  #endif
}

void Turn_OFF_Channel(int card_num, int pin_num)
{
  OffCard = CurrentCard;
  relState = relState_ON;
  #ifdef USE_SERIAL_DEBUG
    Serial.print("State OFF->ON. CardNum:");
    Serial.println(card_num);
  #endif
  digitalWrite(pin_num, RELAY_ON);
  digitalWrite(BUTTON_RELAY, BUTTON_RELAY_ON);
  for (int i = 0; i < card_num + 1; i++) {
    blink_green();
  }
  return;
}

void Bad_Turn_OFF(int card_num)
{
  #ifdef USE_SERIAL_DEBUG
    Serial.print("Bad Turn OFF. CardNum:");
    Serial.println(card_num);
  #endif
  blink_red();
  blink_red();
  blink_red();
  return;
}

void Turn_OFF(int card_num)
{
  switch (card_num) {
    case 0:
      Turn_OFF_Channel(card_num, relay_pins[0]);
      break;
    case 1:
      Turn_OFF_Channel(card_num, relay_pins[1]);
      break;
    case 2:
      Turn_OFF_Channel(card_num, relay_pins[2]);
      break;
    case 3:
      Turn_OFF_Channel(card_num, relay_pins[3]);
      break;
    case 4:
      Turn_OFF_Channel(card_num, relay_pins[4]);
      break;
    default:
      Bad_Turn_OFF(card_num);
    break;  
  }
}

void Turn_ON()
{
  if (CurrentCard != OffCard){
    #ifdef USE_SERIAL_DEBUG
      Serial.print("State ON");
    #endif
    blink_red();
  }else{
    OffCard = 0;
    relState = relState_OFF;
    #ifdef USE_SERIAL_DEBUG
      Serial.print("State ON->OFF");
    #endif
    blink_green();
  }
  return;
}

#define POWER_OFF_BOUNCE_CTR 5
#define POWER_ON_BOUNCE_CTR 5
#define POWER_ON_VOLTAGE_THR 500

void check_power_off()
{
  if (relState != relState_ON) {
    delay(50);
    return;  
  }
  switch (powerState) {
    case powerState_OFF:
      check_power_off_in_off_mode();
      break;
    case powerState_ON:
      check_power_off_in_on_mode();
      break;
    default:
      #ifdef USE_SERIAL_DEBUG
        Serial.print("Unknown power state ");
        Serial.println(powerState);
      #endif
      break;
  }
}

void check_power_off_in_off_mode()
{
  byte ctr = 0;
  for (byte i = 0; i < POWER_ON_BOUNCE_CTR; i++) {
      int rd = analogRead(VOLTAGE_PIN);
      if (rd > POWER_ON_VOLTAGE_THR) {
        ctr += 1;
      } 
      delay(10);
  }
  if (ctr == POWER_ON_BOUNCE_CTR) {
    powerState = powerState_ON; // todo: how to indicate?
    blink_green_red();
  }
}

void check_power_off_in_on_mode()
{
  byte ctr = 0;
  for (byte i = 0; i < POWER_OFF_BOUNCE_CTR; i++) {
      int rd = analogRead(VOLTAGE_PIN);
      if (rd <= POWER_ON_VOLTAGE_THR) {
        ctr += 1;
      } 
      delay(10);
  }
  if (ctr == POWER_OFF_BOUNCE_CTR) {
    Power_OFF();
  }
}

void loop() {
  g_ctr += 1;
  switch (globalState) {
    case gsSimpleMode:
      perform_simple_mode();
      break;  
    case gsMasterMode:
      perform_master_mode();
      break;  
    default:
      #ifdef USE_SERIAL_DEBUG
        Serial.print("Unknown global state ");
        Serial.println(globalState);
      #endif
      break;
  }
  check_power_off();
}
