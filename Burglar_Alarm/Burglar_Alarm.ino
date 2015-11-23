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
 * 
 * 2 (interrupt) : Digital Zone 
 * 3 (interrupt) : Continuous Zone 
 * 4 : LCD Screen 
 * 5 : LCD Screen
 * 6 : LCD Screen
 * 7 : LCD Screen
 * 8 : LCD Screen
 * 9 : LCD Screen
 * 10: Entry/Exit Zone
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
 * Play/Pause : Navigate Log
 *   Next     : Next log
 *   Prev     : Previous log
 * Mute       : Turn off alarm
 * Power      : Set/unset the alarm
 *
 * EEPROM Mapping
 * --------------
 * 0 - 1 : password (unsigned int)
 * 2 - 3 : admin password (unsigned int)
 * 4 - 5 : number of breaches (unsigned short)
 * 6 - 8 : first-time settings ("set" or anything else)
 *
 *  Entry/Exit (Zone 0)
 *    15     : lower-bound hour (unsigned short)
 *    16     : upper-bound hour (unsigned short)
 *
 *  Digital (Zone 1)
 *    20    : trip condition (unsigned short)
 *    
 *  Analog  (Zone 2)
 *    30 - 32 : threshold (unsigned int)
 *
 * 100 - 511 : Logging
 *   Bit Mapping (6 bytes each):
 *   0 - 3 : time (unsigned long int)
 *   4 - 5 : zone (unsigned short)
 *   
 *   
 */

#define PASSWORD              0     // 4 digit pin
#define ADMIN_PASSWORD        2     // 4 digit pin
#define NUMBER_OF_BREACHES    4
#define FIRST_TIME_SET        6

// ~~~~~ ENTRY / EXIT ZONE ~~~~~
#define ENTRY_EXIT_ZONE       0
#define ENTRY_EXIT_PIN        10
#define LOWER_TIME_BOUND      15     // Hour (2 digits max)
#define UPPER_TIME_BOUND      16     // Hour (2 digits max)

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

 // ~~~~~ TIME SETTING MODE ~~~~
#define HOUR 0
#define MINUTE 1
#define DAY 2
#define MONTH 3
#define YEAR 4

// ~~~~~~~~~ LOGS ~~~~~~~~~~~~~~
#define LOG_MEMORY_START  100
#define LOG_LENGTH        6

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

/**
 * Prints the current time to the LCD
 */
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

/**
 * Pads and prints an integer with 0s
 * @param val Integer to pad
 */
void printWithLeadingZero(int val){
  if(val < 10){
    lcd.print('0');
  }
  lcd.print(val);
}

/**
 * Set the time
 */
void changeTime(){
    TimeElements t;
    time_t newTime;
    breakTime(now(), t);
    
    int settingsMode = 0;
    short exitLoop = 0;

    while( !exitLoop ){
      newTime = makeTime(t);
  
      lcd.clear();
      lcd.setCursor(0,0);

      lcd.print( hour(newTime) );
      lcd.print(':');
      printWithLeadingZero( minute(newTime) );

      lcd.print(' ');

      printWithLeadingZero( day(newTime) );
      lcd.print('/');
      printWithLeadingZero( month(newTime) );
      lcd.print('/');
      lcd.print( year(newTime) );

      lcd.setCursor(0,1);
      
      if(settingsMode == HOUR){
        lcd.print("Setting HOUR");
      } else if(settingsMode == MINUTE){
        lcd.print("Setting MINUTE");
      } else if(settingsMode == DAY){
        lcd.print("Setting DAY");
      } else if(settingsMode == MONTH){
        lcd.print("Setting MONTH");
      } else if(settingsMode == YEAR){
        lcd.print("Setting YEAR");
      }

      irrecv.resume();
      while( !irrecv.decode(&results) ) { /* Wait for input! */ }
      switch(results.value){
                            // +4 should be -1, but here we avoid nevative modulo
        case 0xFF22DD: /* PREV */ settingsMode = (settingsMode+4)%5;       break;
        case 0xFF02FD: /* NEXT */ settingsMode = (settingsMode+1)%5;       break;
        case 0xFFE01F: /*  -  */
            if(settingsMode == HOUR){
              t.Hour--;
            } else if(settingsMode == MINUTE){
              t.Minute--;
            } else if(settingsMode == DAY){
              t.Day--;
            } else if(settingsMode == MONTH){
              t.Month--;
            } else if(settingsMode == YEAR){
              t.Year--;
            }
           break;
        case 0xFFA857: /*  +  */
            if(settingsMode == HOUR){
              t.Hour++;
            } else if(settingsMode == MINUTE){
              t.Minute++;
            } else if(settingsMode == DAY){
              t.Day++;
            } else if(settingsMode == MONTH){
              t.Month++;
            } else if(settingsMode == YEAR){
              t.Year++;
            }
           break;
        case 0xFFB04F: /* RET */ exitLoop = 1; lcd.clear(); break;
      }
    }

    setTime(newTime);
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
      lcd.print( "4 Digit Pin");
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

  // Increase the number of breaches
  number_of_breaches++;
  EEPROM.put( NUMBER_OF_BREACHES, number_of_breaches );

  int memory_address = LOG_MEMORY_START + (( LOG_MEMORY_START + (LOG_LENGTH * number_of_breaches) ) % 500);

  // Write our log to EEPROM
  EEPROM.put( memory_address, time_of_breach );
  memory_address += sizeof(time_of_breach);
  EEPROM.put( memory_address, (short) zone );
}

/**
 * Allows user to navigate the stored log
 * @param current_log The current log to be printed
 */
void printLog( short current_log ){
  unsigned short number_of_breaches;
  EEPROM.get( NUMBER_OF_BREACHES, number_of_breaches );

  if( current_log <= number_of_breaches && current_log != 0){
    // If we have a log to show 
    int memory_address = LOG_MEMORY_START + (( LOG_MEMORY_START + (LOG_LENGTH * current_log) ) % 500);
    
    unsigned long int time_of_breach;
    unsigned short zone;

    // Get log info
    EEPROM.get( memory_address, time_of_breach );
    memory_address += sizeof(time_of_breach);
    EEPROM.get( memory_address, zone );
    
    lcd.clear();
    switch(zone){
      case DIGITAL_ZONE:
          lcd.print( "DIGITAL ZONE");
        break;
      case ANALOG_ZONE:
          lcd.print("ANALOG ZONE");
        break;
      case CONTINUOUS_ZONE:
          lcd.print( "CONTINUOUS ZONE");
        break;
      case ENTRY_EXIT_ZONE:
          lcd.print("ENTRY/EXIT ZONE");
        break;
      default:
          lcd.print( "UNKNOWN ZONE" );
        break;
    }
    lcd.setCursor(0,1);
    convertUnixToReadable( time_of_breach );

    irrecv.resume();
    while( !irrecv.decode(&results) ) { /* Wait for input! */ }
    switch(results.value)
    {
      case 0xFF22DD: printLog( current_log - 1 );    break;
      case 0xFF02FD: printLog( current_log + 1 );    break;
      case 0xFFB04F: lcd.clear(); /* If return, just let it go */ break;
      default: printLog(current_log); // Other button press or undefined
    }
    irrecv.resume();
  } else{
    lcd.clear();
    lcd.print("NO LOGS");
    delay( 1000 );

    if( current_log > 0 )
      // If current log isn't 0, send them back a log
      printLog( current_log - 1 );
  }
}

/**
 * Prints out unix time in a human-readable format
 * @param  input_time Unix time input
 */
void convertUnixToReadable( unsigned long int input_time ){
  TimeElements full_time;
  breakTime( input_time, full_time );

  printWithLeadingZero(full_time.Hour);
  lcd.print( ":" );
  printWithLeadingZero(full_time.Minute); 
  lcd.print( " " );
  printWithLeadingZero( full_time.Day );
  lcd.print("/");
  printWithLeadingZero( full_time.Month );
  lcd.print("/");
  lcd.print( full_time.Year + 1970 );
}

/**
 * Exit admin mode
 */
void exitAdmin( ){
  is_admin = 0;
  logout( );
}

/**
 * Remove logged in status
 */
void logout( ){
  is_user_logged_in = 0;
  lcd.clear();
  lcd.print("Logged out");
  delay(700);
}

/**
 * Change whether the alarm can be active or not
 */
void toggleAlarmSet( ){
  if( !alarm_active ){
    lcd.clear();
    // We set the alarm at the end of the function to
    // avoid interrupts triggering the alarm
    unsigned short temp_alarm = !alarm_set;
    
    if( temp_alarm ){
      logout( );
      for (int i = 9; i < 10 && i >= 0; i--){
        lcd.clear();
        lcd.print(i);
        delay(1000);
      }
      lcd.clear();
      lcd.print( "ALARM SET" );
      delay(800);
    } else{
      lcd.print( "ALARM UNSET" );
      delay(800);
    }

    alarm_set = temp_alarm;
  }
}

/**
 * Change whether alarm is ringing or not
 */
void toggleAlarm( ){
  alarm_active = !alarm_active;

  lcd.clear();
  lcd.setCursor(0,1);
  if( alarm_set ){
    if( alarm_active ){
      lcd.print( "ALARM ACTIVE     " );
      digitalWrite( ALARM_PIN, HIGH );
    } else {
      lcd.print( "ALARM DEACTIVATED" );
      digitalWrite( ALARM_PIN, LOW );
      delay(1500);
      lcd.clear();
    }
  }
}


/**
 * Trip the digital zone if conditions are met
 */
void digitalZoneTrip( ){
  volatile unsigned short trip_condition;
  EEPROM.get( DIGITAL_CONDITION, trip_condition );

  if( trip_condition ){
    if( digitalRead( DIGITAL_ZONE_PIN ) == HIGH && !alarm_active && alarm_set ){
      toggleAlarm( );
      appendLog( now(), DIGITAL_ZONE );
    }
  } else {
    if( digitalRead( DIGITAL_ZONE_PIN ) == LOW && !alarm_active && alarm_set ){
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

  if( analogRead( ANALOG_ZONE_PIN ) > threshold && !alarm_active && alarm_set ){
    toggleAlarm( );
    appendLog( now(), ANALOG_ZONE );
    delay(200);
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
        lcd.print("PASSWORD");
        address = PASSWORD;
        digits = 4;
      break;
    case 1:
        lcd.print("ADMIN_PASSWORD");
        address = ADMIN_PASSWORD;
        digits = 4;
      break;
    case 2:
        lcd.print("LOWER TIME (Hour)");
        address = LOWER_TIME_BOUND;
        digits = 2;
      break;
    case 3:
        lcd.print("UPPER TIME (Hour)");
        address = UPPER_TIME_BOUND;
        digits = 2;
      break;
    case 4:
        lcd.print("DIGITAL COND 0/1");
        address = DIGITAL_CONDITION;
        digits = 1;
      break;
    case 5:
        lcd.print("ANALOG THRESH 1-255");
        address = ANALOG_THRESHOLD;
        digits = 3;
      break;
    default:
        return;
      break;
  }

  if( digits > 2 ){
    // If there are more than 2 digits, we'll need an int
    unsigned int final_value = 0;
    lcd.setCursor(0,1);
    for(int i = 0; i < digits; i++){
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
        final_value *= 10;
        final_value += received_value;
        lcd.print(received_value);
        // Minor delay to prevent debouncing "0"s
        delay(50);
        irrecv.resume();
    }
    EEPROM.put( address, final_value );
  } else {
    // If there are less than 2 digits, we can use a short
    unsigned short final_value = 0;
    lcd.setCursor(0,1);
    for(int i = 0; i < digits; i++){
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
        final_value *= 10;
        final_value += received_value;
        lcd.print(received_value);
        // Minor delay to prevent debouncing "0"s
        delay(50);
        irrecv.resume();
    }
    EEPROM.put( address, final_value );
  }
}

/**
 * Places default values in memory if it's the 
 * first time
 */
void defaults(){
  unsigned int password = 1234, 
               admin_password = 5678, 
               analog_threshold = 100;

  unsigned short number_breaches = 0,
                 lower_bound_hour = 20,
                 upper_bound_hour = 22,
                 digital_trip_condition = 1;

  char first_time[] = "SET";
  EEPROM.put( FIRST_TIME_SET, first_time );
  EEPROM.put( PASSWORD, password );
  EEPROM.put( ADMIN_PASSWORD, admin_password );
  EEPROM.put( ANALOG_THRESHOLD, analog_threshold );

  EEPROM.put( NUMBER_OF_BREACHES, number_breaches );
  EEPROM.put( LOWER_TIME_BOUND, lower_bound_hour );
  EEPROM.put( UPPER_TIME_BOUND, upper_bound_hour );
  EEPROM.put( DIGITAL_CONDITION, digital_trip_condition );
}

/**
 * Check if settings exist
 * @return 0 if not first time; 1 if first time
 */
int settingsSet( ){
  char first_time[3];

  EEPROM.get( FIRST_TIME_SET, first_time );

  return !(first_time == "SET");
}

void setup() {
  Serial.begin(9600);
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

  digitalWrite( ALARM_PIN, LOW );
  digitalWrite( CONTINUOUS_ZONE_PIN, LOW );
  digitalWrite( DIGITAL_ZONE_PIN, LOW );

  attachInterrupt( digitalPinToInterrupt(DIGITAL_ZONE_PIN), digitalZoneTrip, CHANGE  );
  attachInterrupt( digitalPinToInterrupt(CONTINUOUS_ZONE_PIN), contZoneTrip, FALLING );

  int first_time = settingsSet( );

  if( first_time ){
    defaults();
  }

  alarm_set = 0;
  
  setTime( 1447854337 );
      
  irrecv.enableIRIn(); 

  lcd.begin(16, 2);
}

void loop() {

  /**
   * Trip the Entry/Exit zone as necessary
   */
  if( digitalRead(ENTRY_EXIT_PIN) == HIGH ){
    unsigned short lower, upper, currentHour;
    EEPROM.get( LOWER_TIME_BOUND, lower );
    EEPROM.get( UPPER_TIME_BOUND, upper );
    currentHour = hour();

    lcd.clear();
    lcd.print("Plz login (EQ)");
    long start_time = millis();
    lcd.setCursor(0,1);
    while( (millis() - start_time) < 20000 ){
       
      irrecv.resume();
      delay(300);
      if(irrecv.decode(&results)){  
        if( results.value == 0xFF906F ){
          if( !is_user_logged_in ){
           loginMode( );
          }
          lcd.clear();
          lcd.print("Crisis averted");
          delay(800);
          break;
        }
      }
      lcd.clear();
      lcd.print("Plz login (EQ)");
      lcd.setCursor(0,1);
      lcd.print( 20 - ((millis() - start_time) / 1000) );
    }
    irrecv.resume(); 

    if( !alarm_active && alarm_set && !( currentHour <= lower && currentHour >= upper) ){
      // If current hour is not between the upper and lower bound
      // then activate the alarm
      toggleAlarm( );
      appendLog( now(), ENTRY_EXIT_ZONE );
      delay( 200 );
    }
  }
  
  analogZoneTrip( );

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
      case 0xFFE21D:
          // MUTE
          if( alarm_active && ( is_user_logged_in || loginMode() ) ){
            toggleAlarm();
          }
        break;
      case 0xFFC23D: 
          /* PLAY/PAUSE */ 
          if( is_user_logged_in || loginMode() )
            printLog( 1 ); 
        break;
      case 0xFF9867:
          /* 100+ */
          if(is_user_logged_in || loginMode() ){
            changeTime();
          }
        break;

      case 0xFFB04F:
          // RET
          if( is_admin ){
            exitAdmin( );
          } else if( is_user_logged_in ){
            logout();
          }
        break;

      /* SET OPTION VALUES */
      case 0xFF6897: 
          if( is_admin || ( loginMode( ) && is_admin ) )
            setOption( 0 );
        break;
      case 0xFF30CF: 
          if( is_admin || ( loginMode( ) && is_admin ) )
            setOption( 1 );
        break;
      case 0xFF18E7: 
          if( is_admin || ( loginMode( ) && is_admin ) )
            setOption( 2 );
        break;
      case 0xFFFFFF: 
          if( is_admin || ( loginMode( ) && is_admin ) )
            setOption( 3 );
        break;
      case 0xFF10EF: 
          if( is_admin || ( loginMode( ) && is_admin ) )
            setOption( 4 );
        break;
      case 0xFF38C7: 
          if( is_admin || ( loginMode( ) && is_admin ) )
            setOption( 5 );
        break;

      default: Serial.println("unrecognised");
    }

    irrecv.resume(); // Receive the next value
  } else {
      printTime();
  }

}
