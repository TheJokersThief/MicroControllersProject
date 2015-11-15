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
 * 1 : Entry/Exit Zone
 * 2 (interrupt) : Digital Zone 
 * 3 (interrupt) : Continuous Zone 
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
 * 4     : number of breaches (unsigned short)
 *
 *  Entry/Exit (Zone 0)
 *    5     : lower-bound hour (unsigned short)
 *    6     : upper-bound hour (unsigned short)
 *
 *  Digital (Zone 1)
 *    20    : trip condition (unsigned short)
 *    
 *  Analog  (Zone 2)
 *    30 - 32 : threshold (unsigned int)
 *
 * 100 - 1024 : Logging
 *   Bit Mapping (5 bytes each):
 *   0 - 3 : time (unsigned long int)
 *   4     : zone (unsigned short)
 *   
 *   
 */

#define PASSWORD              0     // 4 digit pin
#define ADMIN_PASSWORD        2     // 4 digit pin
#define NUMBER_OF_BREACHES    4

// ~~~~~ ENTRY / EXIT ZONE ~~~~~
#define ENTRY_EXIT_ZONE       0
#define ENTRY_EXIT_PIN        1
#define LOWER_TIME_BOUND      5     // Hour (2 digits max)
#define UPPER_TIME_BOUND      6     // Hour (2 digits max)

// ~~~~~~~ DIGITAL ZONE ~~~~~~~~
#define DIGITAL_ZONE          1
#define DIGITAL_CONDITION     20    // HIGH (1) or LOW (0)
#define DIGITAL_ZONE_PIN      2

 // ~~~~~~~ ANALOG ZONE ~~~~~~~~
#define ANALOG_ZONE           2
#define ANALOG_THRESHOLD      30    // short between 0 - 255
#define ANALOG_ZONE_PIN       0


 // ~~~ CONTINUOUS MON ZONE ~~~~
#define CONTINUOUS_ZONE       3
#define CONTINUOUS_ZONE_PIN   2

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
unsigned short is_admin = 0;

// 0 is disabled ; 1 is enabled
unsigned short alarm_set = 0;

// 0 alarm is idle ; 1 alarm is ringing
volatile unsigned short alarm_active = 0;

// 0 not logged in ; 1 logged in 
unsigned short is_user_logged_in = 0;


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

/**
 * Attempt to log in a user, prompting
 *   them for a user or admin password
 * @return 1 if user logged in ; 0 otherwise
 */
int loginMode() {
  lcd.clear();
  lcd.print( "Login Mode");
  if( !is_user_logged_in || !is_admin ){
    // if admin is already logged in, bypass login

    lcd.clear();
    if( is_user_logged_in && !is_admin ){
      lcd.print( "Enter admin pin");
    } else {
      lcd.print( "Enter 4 digit pin");
    }
    delay(50);
    
    int pin_entered = 0;
    unsigned int password, admin_password;
    EEPROM.get( PASSWORD, password );
    EEPROM.get( ADMIN_PASSWORD, admin_password );

    lcd.setCursor(0, 1);
    for(int i = 0; i < 4; i++){
      int received_value = 0;
      irrecv.resume();
      while( !irrecv.decode(&results) ) { /* Wait for input! */ }
      switch(results.value)
      {
        case 0xFF6897: received_value = 0;      break;
        case 0xFF30CF: received_value = 1;      break;
        case 0xFF18E7: received_value = 2;      break;
        case 0xFF7A85: received_value = 3;      break;
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
      lcd.print('*');

      // Minor delay to prevent debouncing "0"s
      delay(50);
      irrecv.resume();
    }


    lcd.setCursor(0,1);
    if( !is_user_logged_in && pin_entered == password ){
      is_user_logged_in = 1;
      lcd.print( "LOGGED IN" );
    } else if( pin_entered == admin_password ){
      is_admin = 1;
      is_user_logged_in = 1;
      lcd.print( "LOGGED IN" );
    } else{
      lcd.print( "FAILED LOGIN" );
    }
    delay(1500);
  } else{
    lcd.print("You are admin");
  }
  lcd.clear();
  
  return is_user_logged_in;
}

/**
 * Append log to memory
 * @param time_of_breach Unix timestamp of current time
 * @param zone           Zone number that was breached
 */
void appendLog( unsigned long int time_of_breach, unsigned short zone ){

  unsigned short number_of_breaches;
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
  lcd.clear();
  lcd.print("Logged out");
}

void toggleAlarmSet( ){
  alarm_set = !alarm_set;
}

/**
 * Change whether alarm is ringing or not
 */
void toggleAlarm( ){
  alarm_active = !alarm_active;

  lcd.clear();
  if( alarm_active ){
    lcd.print( "ALARM ACTIVE     ");
    digitalWrite( ALARM_PIN, HIGH );
  } else {
    lcd.print( "ALARM DEACTIVATED");
    digitalWrite( ALARM_PIN, LOW );
  }
}

/**
 * Trip the entry/exit zone
 */
void entryExitZoneTrip( ){
  volatile unsigned short lower, upper, currentHour;
  EEPROM.get( LOWER_TIME_BOUND, lower );
  EEPROM.get( UPPER_TIME_BOUND, upper );
  currentHour = hour();

  if( !( currentHour <= lower && currentHour >= upper) ){
    // If current hour is not between the upper and lower bound
    // then activate the alarm
    toggleAlarm( );
    appendLog( now(), ENTRY_EXIT_ZONE );
  }
}

/**
 * Trip the digital zone if conditions are met
 */
void digitalZoneTrip( ){
  volatile unsigned short trip_condition;
  EEPROM.get( DIGITAL_CONDITION, trip_condition );

  if( trip_condition ){
    if( digitalRead( DIGITAL_ZONE_PIN ) == HIGH && !alarm_active ){
      toggleAlarm( );
      appendLog( now(), DIGITAL_ZONE );
    }
  } else {
    if( digitalRead( DIGITAL_ZONE_PIN ) == LOW && !alarm_active ){
      toggleAlarm( );
      appendLog( now(), DIGITAL_ZONE );
    }
  }

}

/**
 * Trip the continuous zone
 */
void contZoneTrip( ){
  toggleAlarm( );
  appendLog( now(), CONTINUOUS_ZONE );
}

/**
 * Trip the analog zone if higher than threshold
 */
void analogZoneTrip( ){
  unsigned int threshold;
  EEPROM.get( ANALOG_THRESHOLD, threshold );

  if( analogRead( ANALOG_ZONE_PIN ) > threshold && !alarm_active ){
    toggleAlarm( );
    appendLog( now(), ANALOG_ZONE );
  }
}

/**
 * Allows users to set permanent option 
 *   values (stored in EEPROM)
 * @param option Option number from IR Remote
 */
void setOption( short option ){
  unsigned int address;
  unsigned short digits;
  switch( option ){
    case 0:
        // PASSWORD
        address = PASSWORD;
        digits = 4;
      break;
    case 1:
        // ADMIN_PASSWORD
        address = ADMIN_PASSWORD;
        digits = 4;
      break;
    case 2:
        // LOWER_TIME_BOUND
        address = LOWER_TIME_BOUND;
        digits = 2;
      break;
    case 3:
        // UPPER_TIME_BOUND
        address = UPPER_TIME_BOUND;
        digits = 2;
      break;
    case 4:
        // DIGITAL_CONDITION
        address = DIGITAL_CONDITION;
        digits = 1;
      break;
    case 5:
        // ANALOG_THRESHOLD
        address = ANALOG_THRESHOLD;
        digits = 2;
      break;
    default:
        return;
      break;
  }

  if( digits > 2 ){
    // If there are more than 2 digits, we'll need an int
    unsigned int final_value;
    for(int i = 0; i < digits; i++){
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
        final_value *= 10;
        final_value += received_value;
    }

    EEPROM.put( address, final_value );
  } else {
    // If there are less than 2 digits, we can use a short
    unsigned short final_value;
    for(int i = 0; i < digits; i++){
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
        final_value *= 10;
        final_value += received_value;
    }

    EEPROM.put( address, final_value );
  }
}



void setup() {
  // Interrupts once per second
  // TCCR1A = 0;
  // TCCR1B = 0;
  // OCR1A = 15625;
  // TCCR1B |= (1 << WGM12);
  // TCCR1B |= (1 << CS10);
  // TCCR1B |= (1 << CS12);
  // TIMSK1 |= (1 << OCIE1A);
  sei();

  pinMode(ALARM_PIN, OUTPUT);
  pinMode(CONTINUOUS_ZONE_PIN, INPUT);
  pinMode(DIGITAL_ZONE_PIN, INPUT);
  pinMode(ENTRY_EXIT_PIN, INPUT);

  attachInterrupt( digitalPinToInterrupt(DIGITAL_ZONE_PIN), digitalZoneTrip, CHANGE  );
  attachInterrupt( digitalPinToInterrupt(CONTINUOUS_ZONE_PIN), contZoneTrip, FALLING );

  Serial.begin(9600);

  setTime(1262347200);
  
  Serial.println( now() );
  
  irrecv.enableIRIn(); 

  lcd.begin(16, 2);
}

void loop() {
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
          if( is_admin ){
            exitAdmin( );
          } else if( is_user_logged_in ){
            logout();
          }
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
  } else {
      printTime();
  }

  if( digitalRead(ENTRY_EXIT_PIN) == HIGH ){
    unsigned short lower, upper, currentHour;
    EEPROM.get( LOWER_TIME_BOUND, lower );
    EEPROM.get( UPPER_TIME_BOUND, upper );
    currentHour = hour();

    if( !alarm_active && !( currentHour <= lower && currentHour >= upper) ){
      // If current hour is not between the upper and lower bound
      // then activate the alarm
      toggleAlarm( );
      appendLog( now(), ENTRY_EXIT_ZONE );
    }
  }

}

ISR (TIMER1_COMPA_vect){
//  printTime();
}
