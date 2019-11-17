#include <SPI.h>
// RFID Library (using package manager). v1.4.5
#include <MFRC522.h> 
#include <EEPROM.h>
#define SS_PIN 10
#define RST_PIN 9
#define Led1 4
#define RELAY_PIN 5

unsigned long g_ctr = 0;
unsigned long c_ctr = 0;
byte ee_cards = 0;
bool new_state_event = false;
#define MAX_CARDS 10
#define EMPTY_CARD 0xffffffff
#define CARDS_CTR_ADDRESS 0
#define FIRST_CARD_ADDRESS 1

MFRC522 mfrc522(SS_PIN, RST_PIN);

enum ReadCardStates {
  rcState_Init,
  rcState_Present,
  rcState_Read,
  rcState_Ready,  
  rcState_Ready2  
};

ReadCardStates rcState = rcState_Init;
unsigned long CurrentCard = 0;
  
void setup() {
  Serial.begin(115200);
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
  if (rcState == rcState_Ready2 && new_card == CurrentCard) {
    c_ctr = 0;
    g_ctr = 0; // Reset counter;
    return;
  }
  if ( change_state(rcState_Ready, "Card!") ) {
    CurrentCard = new_card;
    c_ctr = 0;
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
      Serial.print("Delete Card ");
      Serial.print(new_card);
      Serial.print(" at ");
      Serial.println(num);
      delete_card(num, new_card);
      dump_eeprom();
    }
  }

}

void loop() {
  g_ctr += 1;
  // put your main code here, to run repeatedly:
  check_card();
  delay(50);
}
