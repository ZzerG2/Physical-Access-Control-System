#include <SPI.h>
#include <MFRC522.h> // библиотека "RFID".
#define SS_PIN 10
#define RST_PIN 9
#define Led1 4
#define RELAY_PIN 5
byte Mode = LOW;

unsigned long Ctr = 0;

unsigned long MasterCard = 3507708549;
unsigned long CurrentCard = 0;
unsigned long uidDecTemp, uidDec, uidNewTemp, uidNew;
#define MAX_CARD 5
unsigned long cards_array[MAX_CARD]; 
MFRC522 mfrc522(SS_PIN, RST_PIN);
enum States
{
  State_wait,
  State_check,
  State_compcard,
  State_compcard2,
  State_goodcard,  
  State_badcard,  
  State_record,
  State_record2  
};
States state = State_wait;
  
void setup() {
  for (int i = 0; i < MAX_CARD; i++) {
    cards_array[i] = 0;
  }
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();  
  set_wait();
  pinMode(Led1, OUTPUT);
}

unsigned long read_card()
{
    // read RFID
    uidDec = 0;
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec * 256 + uidDecTemp;
    }
    return uidDec;
}

void set_state(States new_state) {
  Serial.print (state);  
  Serial.print ("->");  
  Serial.println (new_state);
  state = new_state;
}

void set_wait()
{
  Ctr = 0;
   state = State_wait;  
   Serial.println("Поднесите карту");
}

void perform_wait()
{
        if ( ! mfrc522.PICC_IsNewCardPresent()) {
            if (Ctr >= 20) {
              Ctr = 20;
              CurrentCard = 0;  
            }
            else {
              Ctr += 1;  
            }
            return;
        }
        if ( ! mfrc522.PICC_ReadCardSerial()) {
            Serial.println("Wait-2");
            return;
        }
        Ctr = 0;
        Serial.println("Wait-3");
        Serial.print("CC:");
        Serial.print(CurrentCard);
        Serial.println("");
        unsigned long new_card = read_card();
        Serial.print("NC:");
        Serial.print(new_card);
        Serial.println("");
        if (new_card == MasterCard && CurrentCard == MasterCard) {
          Serial.println("MasterCard Duplicate");
          return;  
        }
        // compare
        if (new_card == MasterCard) {
          Serial.println("Prepare Master Mode");
          CurrentCard = new_card;
          set_state(State_compcard);
        }
        else {
          Serial.println("Operational Mode");
          CurrentCard = new_card;
          set_state(State_check);
        }
}

void perform_check()
{
  for (int i = 0; i < MAX_CARD; i++) {
    if (CurrentCard == cards_array[i]) {
      set_state(State_goodcard);
      return;
    }  
  }
  set_state(State_badcard);
}

void perform_badcard()
{
  // set bad light on
  // delay
  Serial.print("Perform Bad ");
  Serial.println(CurrentCard);
  delay(1000);
  CurrentCard = 0;
  set_wait();
}


void perform_goodcard()
{
  Serial.print("Perform Good ");
  Serial.println(CurrentCard);
  // set good light on or perform an action
  if (Mode == HIGH) {
    Mode = LOW;  
  }
  else {
    Mode = HIGH;  
  }
  digitalWrite(RELAY_PIN, Mode);
  // delay
  delay(1000);
  CurrentCard = 0;
  set_wait();
}

void perform_compcard()
{
    if ( !mfrc522.PICC_IsNewCardPresent()) {
        if (state != State_compcard2) {
          set_state (State_compcard2);
        }
        return;
    }
    Serial.println("CompCard Loop");
    delay(100);
}

void perform_compcard2()
{
    if ( !mfrc522.PICC_IsNewCardPresent()) {
        return;
    }
    if ( ! mfrc522.PICC_ReadCardSerial()) {
        return;
    }
    unsigned long new_card = read_card();
    if (new_card == CurrentCard && new_card == MasterCard) {
      // change mode
      set_state(State_record);
      return;
    }
    else {
      CurrentCard = 0;
      set_wait();
      return;  
    }
}

void perform_record()
{
  if ( !mfrc522.PICC_IsNewCardPresent()) {
      return;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
      return;
  }
  unsigned long new_card = read_card();
  if (new_card == MasterCard) {
    digitalWrite(Led1, HIGH);
    Serial.println("Master Card-2");
    delay(100);
    return;  
  }
  if (new_card == 0) {
    Serial.println("0-Card");
    delay(100);
    return;  
  }
  for (int i = 0; i < MAX_CARD; i++) {
    if (new_card == cards_array[i]) {
      Serial.print("Found card ");
      Serial.print(new_card);
      Serial.print(" at ");
      Serial.println(i);
      set_state(State_record2);
      return;
    }  
  }
  for (int i = 0; i < MAX_CARD; i++) {
    if (cards_array[i] == 0) {
      Serial.print("Write new card ");
      Serial.print(new_card);
      Serial.print(" to ");
      Serial.println(i);
      cards_array[i] = new_card;
      if (i == MAX_CARD-1) {
        Serial.println("Out of memory");
        digitalWrite(Led1, LOW);
        CurrentCard = 0;
        set_wait(); // temporary decision
        return;
      }
      set_state(State_record2);
      return;
    }  
  }
  // out of memory!!!
  Serial.println("Out of memory");
  CurrentCard = 0;
  set_wait(); // temporary decision
  return;
}

void perform_record2()
{
  if ( !mfrc522.PICC_IsNewCardPresent()) {
      return;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
      return;
  }
  unsigned long new_card = read_card();
  if (new_card == MasterCard) {
    Serial.println("Found Master Card");
    digitalWrite(Led1, LOW);
    CurrentCard = MasterCard;
    set_wait();
    return;  
  }
  if (new_card == 0) {
    Serial.println("0-Card");
    delay(100);
    return;  
  }
  for (int i = 0; i < MAX_CARD; i++) {
    if (new_card == cards_array[i]) {
      Serial.print("Found card ");
      Serial.print(new_card);
      Serial.print(" at ");
      Serial.println(i);
      return;
    }  
  }
  for (int i = 0; i < MAX_CARD; i++) {
    if (cards_array[i] == 0) {
      Serial.print("Write new card ");
      Serial.print(new_card);
      Serial.print(" to ");
      Serial.println(i);
      cards_array[i] = new_card;
      if (i == MAX_CARD-1) {
        Serial.println("Out of memory");
        digitalWrite(Led1, LOW);
        CurrentCard = 0;
        set_wait(); // temporary decision
        return;
      }
      return;
    }  
  }
  // out of memory!!!
  Serial.println("Out of memory");
  digitalWrite(Led1, LOW);
  CurrentCard = 0;
  set_wait(); // temporary decision
  return;
}

void loop() {
  // put your main code here, to run repeatedly:
  switch (state)
  {
    case State_wait: //Ожидание карты -> Compare or Check
         perform_wait();
         break;
    case State_check:
         perform_check();
         break;
    case State_badcard:
         perform_badcard();
         break;
    case State_goodcard:
         perform_goodcard();
         break;
    case State_compcard:    
         perform_compcard();
         break;
    case State_compcard2:    
         perform_compcard2();
         break;
    case State_record:    
         perform_record();
         break;
    case State_record2:    
         perform_record2();
         break;
    default:
         Serial.print("Unknown State ");
         Serial.println(state);
         delay(1000);
         break;
  }
  delay(100);
}
void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
