#include <Arduino.h>
#include <EEPROM.h>

bool debug = 0;
const int downButtonPin = 11;
const int upButtonPin = 12;
const int grindButtonPin = 13;

const int secs_16 = 10;
const int secs_8 = 9;
const int secs_4 = 8;
const int secs_2 = 7;
const int secs_1 = 6;

const int tenths_secs_8 = 5;
const int tenths_secs_4 = 4;
const int tenths_secs_2 = 3;
const int tenths_secs_1 = 2;

const int grindActivatePin = A0;

int upButtonState = LOW;
int downButtonState = LOW;
int grindButtonState = LOW;

int upButtonPrevState = LOW;
int downButtonPrevState = LOW;
int grindButtonPrevState = LOW;

bool upButtonPressed = false;
bool downButtonPressed = false;
bool grindButtonPressed = false;
  
int eeAddress = 0; //EEPROM address to start reading from
float grind_time = 0.0f; //Variable to store data read from EEPROM.

float remaining_grind_time;
float set_time_incriment = 0.1;
// given 5 bit binary display, 32 secs cannot be displayed
// use max/min grind time to muliples of set_time_incriment to avoid
// jumps in set time if limits are hit
float max_grind_time = 32 - set_time_incriment;
float min_grind_time = set_time_incriment;


int secs;
int tenths_secs;

unsigned long interval = 100; // the time we need to wait
unsigned long previousMillis = 0; // millis() returns an unsigned long.

int setup_time_counter = 0;
int setup_time_limit = 200;

enum State_enum {WAITING, SET_TIME, GRIND};
enum Button_enum {NONE, BUTTON_UP, BUTTON_DOWN, BOTH, BUTTON_GRIND};

uint8_t button_management();
uint8_t buttons;

uint8_t state = WAITING;

// Method forward declarations
void setup();
void state_machine_run();
void display_all_on();
void blink_display();
void deactivate_display();
void display_time(float disp_time);
void countdown_grind_time();
uint8_t button_management();
void loop();


void setup() {
  // put your setup code here, to run once:
  pinMode(upButtonPin, INPUT);
  pinMode(downButtonPin, INPUT);
  pinMode(grindButtonPin, INPUT);
  
  pinMode(grindActivatePin, OUTPUT);

  pinMode(secs_16, OUTPUT);
  pinMode(secs_8, OUTPUT);
  pinMode(secs_4, OUTPUT);
  pinMode(secs_2, OUTPUT);
  pinMode(secs_1, OUTPUT);

  pinMode(tenths_secs_8, OUTPUT);
  pinMode(tenths_secs_4, OUTPUT);
  pinMode(tenths_secs_2, OUTPUT);
  pinMode(tenths_secs_1, OUTPUT);

  //Get the float data from the EEPROM at position 'eeAddress'
  EEPROM.get( eeAddress, grind_time );
  if(debug){Serial.println( grind_time, 1 );}  //This may print 'ovf, nan' if the data inside the EEPROM is not a valid float.

  // initialize serial communication:
  if(debug){Serial.begin(9600);}

}

void state_machine_run()
 {
   switch(state)
   {
      case WAITING:
        buttons = button_management();
      
        if(buttons == BOTH)
        {
          if(debug){Serial.println("enter set time mode");}
          display_time(grind_time);
          state = SET_TIME;
        }
        else if(buttons == BUTTON_GRIND)
        {
          if(debug){Serial.println("enter grind mode from waiting");}
          state = GRIND;
          digitalWrite(grindActivatePin, HIGH);
          remaining_grind_time = grind_time;
          display_time(remaining_grind_time);
        }
        break;
        
      case SET_TIME:
        buttons = button_management();

        // TO DO: TURN INDICATOR LIGHTS ON when entering set mode
        // TO DO: turn off lights when returning to wait mode
        
        if(buttons == BUTTON_UP)
           {
            grind_time = grind_time + set_time_incriment;            
            setup_time_counter = 0;

            if(grind_time > max_grind_time)
             {
              grind_time = max_grind_time;
              blink_display();
             }

             display_time(grind_time);
             EEPROM.put(eeAddress, grind_time);
           }
        
        else if(buttons == BUTTON_DOWN)
           {
            grind_time = grind_time - set_time_incriment;            
            setup_time_counter = 0;

            if(grind_time < min_grind_time)
             {
              grind_time = min_grind_time;
              blink_display();
             }

             display_time(grind_time);
             EEPROM.put(eeAddress, grind_time);
           }

        else if(buttons == BOTH || setup_time_counter > setup_time_limit)
           {
            state = WAITING;
            deactivate_display();
            setup_time_counter = 0;
           }

        else if(buttons == BUTTON_GRIND)
           {
            if(debug){Serial.println("enter grind mode from set time");}
            state = GRIND;
            digitalWrite(grindActivatePin, HIGH);
            remaining_grind_time = grind_time;
            display_time(remaining_grind_time);
           }
        setup_time_counter++;        
        break;

      case GRIND:
        
        countdown_grind_time();
        break;
   }
 }

void display_all_on(){

  digitalWrite(secs_16, HIGH);
  digitalWrite(secs_8, HIGH);
  digitalWrite(secs_4, HIGH);
  digitalWrite(secs_2, HIGH);
  digitalWrite(secs_1, HIGH);

  digitalWrite(tenths_secs_8, HIGH);
  digitalWrite(tenths_secs_4, HIGH);
  digitalWrite(tenths_secs_2, HIGH);
  digitalWrite(tenths_secs_1, HIGH);
  
}

void blink_display(){
  int a;
  for( a = 0; a < 3; a++ ){
    display_all_on();
    delay(100);
    deactivate_display();
    delay(100);
  }
}

void deactivate_display(){
  // turn all display pins to zero

  digitalWrite(secs_16, LOW);
  digitalWrite(secs_8, LOW);
  digitalWrite(secs_4, LOW);
  digitalWrite(secs_2, LOW);
  digitalWrite(secs_1, LOW);

  digitalWrite(tenths_secs_8, LOW);
  digitalWrite(tenths_secs_4, LOW);
  digitalWrite(tenths_secs_2, LOW);
  digitalWrite(tenths_secs_1, LOW);
}

void display_time(float disp_time){
  // display current set time with LEDs

  // seperate seconds and tenths of seconds into two variables
  // round to 1 decimal place to aviod issue that: 
  // (int)1.9999 = 1, but I want it to equal 2 
  secs = (int)round(disp_time*10)/10;
  tenths_secs = round((disp_time - secs)*10.0);

  // write seconds to display LEDs
  digitalWrite(secs_16, HIGH && (secs & B00010000));
  digitalWrite(secs_8, HIGH && (secs & B00001000));
  digitalWrite(secs_4, HIGH && (secs & B00000100));
  digitalWrite(secs_2, HIGH && (secs & B00000010));
  digitalWrite(secs_1, HIGH && (secs & B00000001));

  // write tenths of seconds to display LEDs
  digitalWrite(tenths_secs_8, HIGH && (tenths_secs & B00001000));
  digitalWrite(tenths_secs_4, HIGH && (tenths_secs & B00000100));
  digitalWrite(tenths_secs_2, HIGH && (tenths_secs & B00000010));
  digitalWrite(tenths_secs_1, HIGH && (tenths_secs & B00000001));
  
  if(debug){
    Serial.println("float");
    Serial.println(disp_time*10000000);
    Serial.println("secs");
    Serial.println(secs);      
    Serial.println("tenths_secs");
    Serial.println(tenths_secs); 
 
         
  }
  
}

void countdown_grind_time(){

   // Checking whether time is up or not
  if(remaining_grind_time <= 0.0){
    digitalWrite(grindActivatePin, LOW);
    deactivate_display();
    state = WAITING;
   }
  else
  {
    // countdown remaining_grind_time
    // update remaining_time if interval has elapsed
    
    unsigned long currentMillis = millis(); // grab current time
 
    // check if "interval" time has passed (100 milliseconds)
    if ((unsigned long)(currentMillis - previousMillis) >= interval) {
       remaining_grind_time = remaining_grind_time - interval/1000.0;
       previousMillis = millis();
       display_time(remaining_grind_time);
       }  
  }
  
}


uint8_t button_management(){
   // Delay a little bit to avoid bouncing and be able to detect 
   // when both up and down button is pressed!
   
   
   enum Button_enum Button_action;

   Button_action = NONE;
   
  /*
   * Down button management
   */
  delay(25);
  
  downButtonPressed = false;
  downButtonState = digitalRead(downButtonPin);
  if(downButtonState != downButtonPrevState)
  {
    downButtonPressed = downButtonState == HIGH;
    downButtonPrevState = downButtonState;
  }

  delay(50);
  
  /*
   * Up button management
   */
  upButtonPressed = false;
  upButtonState = digitalRead(upButtonPin);
  if(upButtonState != upButtonPrevState)
  {
    upButtonPressed = upButtonState == HIGH;
    upButtonPrevState = upButtonState;
  }
  delay(35);

  /*
   * Grind button management
   */
  grindButtonPressed = false;
  grindButtonState = digitalRead(grindButtonPin);
  if(grindButtonState != grindButtonPrevState)
  {
    grindButtonPressed = grindButtonState == HIGH;
    grindButtonPrevState = grindButtonState;
  }
  
  if(downButtonPressed && !upButtonPressed)
     {if(debug){Serial.println("down");}
      Button_action = BUTTON_DOWN;
      }

 if(upButtonPressed && !downButtonPressed)
     {if(debug){Serial.println("up");}
      Button_action = BUTTON_UP;
      }

 if(upButtonPressed && downButtonPressed)
     {if(debug){Serial.println("both");}
     Button_action = BOTH;
     }

 if(grindButtonPressed)
     {if(debug){Serial.println("grind button pressed");}
     Button_action = BUTTON_GRIND;
     }

  return Button_action;    
}


void loop() {
  // put your main code here, to run repeatedly:
 
 state_machine_run();
 delay(10);
}