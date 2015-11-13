#include <Time.h>  
#include <IRremote.h>
#include <LiquidCrystal.h>

/*
 * Pins
 * ====
 * 
 * Digital
 * -------
 * 1 (interrupt) : Digital Zone 1 
 * 
 * 4 : LCD Screen 
 * 5 : LCD Screen
 * 6 : LCD Screen
 * 7 : LCD Screen
 * 8 : LCD Screen
 * 9 : LCD Screen
 * 
 * 12: IR Sensor
 * 13: ALARM
 * 
 * Analog
 * ------
 * 0 : Analog Zone 1
 *
 * IR Remote Layout
 * ----------------
 *
 * EQ : Enter password (4-Digit Pin)
 * Return : Return to standard menu
 *
 * EEPROM Mapping
 * --------------
 * 0 - 1 : password (unsigned int)
 * 2 - 3 : admin password (unsigned int)
 * 4     : number of breaches (unsigned short int)
 * 100 - 1024 : Logging
 *   Bit Mapping (5 bytes each):
 *   0 - 3 : time (unsigned long int)
 *   4     : zone (unsigned short int)
 *   
 *   
 */

#define PASSWORD              0
#define ADMIN_PASSWORD        2
#define NUMBER_OF_BREACHES    4

#define LOG_MEMORY_START  100
#define LOG_LENGTH        5

#define TIME_MSG_LEN  11   // time sync to PC is HEADER followed by Unix time_t as ten ASCII digits
#define TIME_HEADER  'T'   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

#define IR_RECV_PIN   12
IRrecv irrecv(IR_RECV_PIN);
decode_results results;

#define ALARM_PIN     13

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(9, 8, 7, 6, 5, 4);

// Taken from an example of Time.h, a method to sync time to PC
// using a Unix time value sent by the PC over serial
//void syncTime(){
//  // if time sync available from serial port, update time and return true
//  while(Serial.available() >=  TIME_MSG_LEN ){  // time message consists of header & 10 ASCII digits
//    char c = Serial.read() ; 
//    Serial.print(c);  
//    if( c == TIME_HEADER ) {       
//      time_t pctime = 0;
//      for(int i=0; i < TIME_MSG_LEN -1; i++){   
//        c = Serial.read();          
//        if( c >= '0' && c <= '9'){   
//          pctime = (10 * pctime) + (c - '0') ; // convert digits to a number    
//        }
//      }   
//      setTime(pctime);   // Sync Arduino clock to the time received on the serial port
//    }  
//  }
//}

void printTime(){
  lcd.setCursor(0, 0);

  lcd.print( hour() );
  lcd.print(':');
  printWithLeadingZero( minute() );


  lcd.print( ' ' );

  printWithLeadingZero( day() );
  lcd.print( '/' );
  printWithLeadingZero( month() );
  lcd.print( '/' );
  lcd.print( year() );
}

void printWithLeadingZero(int val){
  if(val < 10){
    lcd.print('0');
  }
  lcd.print(val);
}

void loginMode() {
  int pin_entered = 0;
  for(int i = 0; i < 4; i++){
    int received_value = 0;
    while( !irrecv.decode(&results) ) { /* Wait for input! */ }
    switch(results.value)
    {
      case 0xFF6897: received_value = 0;      break;
      case 0xFF30CF: received_value = 1;      break;
      case 0xFF18E7: received_value = 2;      break;
      case 0xFFFFFF: received_value = 3;      break;
      case 0xFF10EF: received_value = 4;      break;
      case 0xFF38C7: received_value = 5;      break;
      case 0xFF5AA5: received_value = 6;      break;
      case 0xFF42BD: received_value = 7;      break;
      case 0xFF4AB5: received_value = 8;      break;
      case 0xFF52AD: received_value = 9;      break;
      case 0xFFB04F: Serial.println("RET");   break;
      default: i--; break; // Other button press or undefined
    }
    pin_entered *= 10;
    pin_entered += received_value;
  }
}

/**
 * Append log to memory
 * @param time_of_breach Unix timestamp of current time
 * @param zone           Zone number that was breached
 */
void appendLog( unsigned long int time_of_breach, unsigned short int zone ){

  unsigned short int number_of_breaches = EEPROM.read( NUMBER_OF_BREACHES );
  int memory_address = LOG_MEMORY_START + (LOG_LENGTH * number_of_breaches );

  // Increase the number of breaches
  number_of_breaches++;
  EEPROM.write( NUMBER_OF_BREACHES, number_of_breaches );

  // Write our log to EEPROM
  EEPROM.write( memory_address, time_of_breach );
  memory_address += sizeof(time_of_breach);
  EEPROM.write( memory_address, zone );
}

void setup() {
  pinMode(ALARM_PIN, OUTPUT);
  
  Serial.begin(9600);

  setTime(1262347200);
  
  Serial.println( now() );
  
  irrecv.enableIRIn(); 

  lcd.begin(16, 2);
}

void loop() {
  printTime();
  if( irrecv.decode(&results) ) {
    switch(results.value)
    {
      case 0xFF906F: loginMode();                   break;
      case 0xFFA25D: Serial.println("POW");         break;
      case 0xFF629D: Serial.println("MODE");        break;
      case 0xFFE21D: Serial.println("MUTE");        break;
      case 0xFF22DD: Serial.println("PREV");        break;
      case 0xFF02FD: Serial.println("NEXT");        break;
      case 0xFFC23D: Serial.println("PLAY/PAUSE");  break;
      case 0xFFE01F: Serial.println("-");           break;
      case 0xFFA857: Serial.println("+");           break;
      case 0xFF6897: Serial.println("0");           break;
      case 0xFF9867: Serial.println("100+");        break;
      case 0xFFB04F: Serial.println("RET");         break;
      case 0xFF30CF: Serial.println("1");           break;
      case 0xFF18E7: Serial.println("2");           break;
      case 0xFFFFFF: Serial.println("3");           break;
      case 0xFF10EF: Serial.println("4");           break;
      case 0xFF38C7: Serial.println("5");           break;
      case 0xFF5AA5: Serial.println("6");           break;
      case 0xFF42BD: Serial.println("7");           break;
      case 0xFF4AB5: Serial.println("8");           break;
      case 0xFF52AD: Serial.println("9");           break;
      default: Serial.println("unrecognised");
    }

    irrecv.resume(); // Receive the next value
  }
}

