#include <EEPROM.h>
#include <Time.h>  
#include <IRremote.h>
#include <LiquidCrystal.h>

/*
 * Pins
 * ====
 * 
 * Digital
 * -------
 * 2 (interrupt) : Entry/Exit Zone 
 * 3 (interrupt) : Digital Zone 1 
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
 *
 *  Entry/Exit (Zone 0)
 *    5     : lower-bound hour (unsigned short int)
 *    6     : upper-bound hour (unsigned short int)
 *
 *  Digital (Zone 1)
 *    20    : trip condition (unsigned short int)
 *    
 *  Analog  (Zone 2)
 *    30 - 32 : threshold (unsigned int)
 *
 *  Continuous Monitoring (Zone 3)
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

// ~~~~~ ENTRY / EXIT ZONE ~~~~~
#define ENTRY_EXIT_ZONE       0
#define ENTRY_EXIT_PIN        3
#define LOWER_TIME_BOUND      5
#define UPPER_TIME_BOUND      6

// ~~~~~~~ DIGITAL ZONE ~~~~~~~~
#define DIGITAL_ZONE          1
#define DIGITAL_CONDITION     20
#define DIGITAL_ZONE_PIN      2

 // ~~~~~~~ ANALOG ZONE ~~~~~~~~
#define ANALOG_ZONE           2
#define ANALOG_THRESHOLD      30
#define ANALOG_ZONE_PIN       0


 // ~~~ CONTINUOUS MON ZONE ~~~~
#define CONTINUOUS_ZONE       3

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

// Used to check if current user is an admin
unsigned short int is_admin = 0;

// 0 is disabled ; 1 is enabled
unsigned short int alarm_set = 0;

// 0 alarm is idle ; 1 alarm is ringing
unsigned short int alarm_active = 0;

// 0 not logged in ; 1 logged in 
unsigned short int is_user_logged_in = 0;


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

int loginMode() {
  int pin_entered = 0;
  unsigned int password, admin_password;
  EEPROM.get( PASSWORD, password );
  EEPROM.get( ADMIN_PASSWORD, admin_password );

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

  if( pin_entered == password ){
    is_user_logged_in = 1;
  } else if( pin_entered == admin_password ){
    is_admin = 1;
    is_user_logged_in = 1;
  }

  return is_user_logged_in;
}

/**
 * Append log to memory
 * @param time_of_breach Unix timestamp of current time
 * @param zone           Zone number that was breached
 */
void appendLog( unsigned long int time_of_breach, unsigned short int zone ){

  unsigned short int number_of_breaches;
  EEPROM.get( NUMBER_OF_BREACHES, number_of_breaches );
  int memory_address = LOG_MEMORY_START + (LOG_LENGTH * number_of_breaches );

  // Increase the number of breaches
  number_of_breaches++;
  EEPROM.write( NUMBER_OF_BREACHES, number_of_breaches );

  // Write our log to EEPROM
  EEPROM.write( memory_address, time_of_breach );
  memory_address += sizeof(time_of_breach);
  EEPROM.write( memory_address, zone );
}

/**
 * Exit admin mode
 */
void exitAdmin( ){
  is_admin = 0;
  logout( );
}

void logout( ){
  is_user_logged_in = 0;
}

void toggleAlarmSet( ){
  alarm_set = !alarm_set;
}

/**
 * Change whether alarm is ringing or not
 */
void toggleAlarm( ){
  alarm_active = !alarm_active;

  if( alarm_active )
    digitalWrite( ALARM_PIN, HIGH );
  else 
    digitalWrite( ALARM_PIN, LOW );
}

/**
 * Trip the entry/exit zone
 */
void entryExitZoneTrip( ){
  unsigned short lower, upper, currentHour;
  EEPROM.get( LOWER_TIME_BOUND, lower );
  EEPROM.get( UPPER_TIME_BOUND, upper );
  currentHour = hour();

  if( !( currentHour <= lower && currentHour >= upper) ){
    // If current hour is not between the upper and lower bound
    toggleAlarm( );
    appendLog( now(), ENTRY_EXIT_ZONE );
  }
}

void digitalZoneTrip( ){
  unsigned short trip_condition;
  EEPROM.get( DIGITAL_CONDITION, trip_condition );

  if( trip_condition ){
    if( digitalRead( DIGITAL_ZONE_PIN ) == HIGH && !alarm_active )
      toggleAlarm( );
  } else {
    if( digitalRead( DIGITAL_ZONE_PIN ) == LOW && !alarm_active )
      toggleAlarm( );
  }

}

void analogZoneTrip( ){
  unsigned int threshold;
  EEPROM.get( ANALOG_THRESHOLD, threshold );

  if( analogRead( ANALOG_ZONE_PIN ) > threshold && !alarm_active ){
    toggleAlarm( );
  }
}

void setup() {
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 15625;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);
  TIMSK1 |= (1 << OCIE1A);
  sei();

  attachInterrupt( digitalPinToInterrupt(DIGITAL_ZONE_PIN), digitalZoneTrip, CHANGE );
  attachInterrupt( digitalPinToInterrupt(ENTRY_EXIT_PIN), entryExitZoneTrip, CHANGE );

  pinMode(ALARM_PIN, OUTPUT);
  
  Serial.begin(9600);

  setTime(1262347200);
  
  Serial.println( now() );
  
  irrecv.enableIRIn(); 

  lcd.begin(16, 2);
}

void loop() {
  // printTime();
  if( irrecv.decode(&results) ) {
    switch(results.value)
    {
      case 0xFF906F: 
          // EQ
          loginMode();
        break;
      case 0xFFA25D:
          // POW
          if( !alarm_active && ( is_user_logged_in || loginMode() ) )
            toggleAlarmSet( );
        break;
      case 0xFF629D: Serial.println("MODE");        break;
      case 0xFFE21D:
          // MUTE
          if( alarm_active && ( is_user_logged_in || loginMode() ) ){
            toggleAlarm();
          }
        break;
      case 0xFF22DD: Serial.println("PREV");        break;
      case 0xFF02FD: Serial.println("NEXT");        break;
      case 0xFFC23D: Serial.println("PLAY/PAUSE");  break;
      case 0xFFE01F: Serial.println("-");           break;
      case 0xFFA857: Serial.println("+");           break;
      case 0xFF6897: Serial.println("0");           break;
      case 0xFF9867: Serial.println("100+");        break;

      case 0xFFB04F:
          // RET
          if( is_admin )
            exitAdmin( );
        break;
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

ISR (TIMER1_COMPA_vect){
  printTime();
}
