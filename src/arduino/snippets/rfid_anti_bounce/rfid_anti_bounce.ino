#include <SPI.h>
// RFID Library (using package manager). v1.4.5
#include <MFRC522.h> 
#define SS_PIN 10
#define RST_PIN 9
#define Led1 4
#define RELAY_PIN 5
#define MAX_CARD 5

unsigned long g_ctr = 0;
unsigned long c_ctr = 0;

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
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();  
}

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
    return true;
  }  
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
    Serial.print("Found card ");
    Serial.println(new_card);
  }
  else {
    c_ctr += 1;
    g_ctr = 0; // Reset counter;
    // todo: check card number???  
  }
  if (rcState == rcState_Ready && c_ctr > 20) {
    change_state(rcState_Ready2, "Card++");
    CurrentCard = new_card;
  }

}

void loop() {
  g_ctr += 1;
  // put your main code here, to run repeatedly:
  check_card();
  delay(50);
}
