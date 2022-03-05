#include <Arduino.h>
#include <EEPROM.h>
#include "display.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif


char text_buffer[80];
char str_number[20];
unsigned long display_off_delay = 8 * 1000; // time after grind is finished that display stays active
unsigned long time_grind_finished;


bool debug = 1;
const int downButtonPin = 11;
const int upButtonPin = 12;
const int grindButtonPin = 4;

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
float set_grind_weight = 10.0; //Variable to store data read from EEPROM.
float previous_set_grind_weight = set_grind_weight;
float set_grind_weight_eeprom = 0.0f;

float current_weight = 0;
float set_weight_incriment = 0.1;
float max_grind_weight = 100;
float min_grind_weight = 2;
unsigned long time_last_setup_b_press;
unsigned long time_exit_setup_after_last_presss = 5 * 1000;

int secs;
int tenths_secs;

double interval = 3;
double previousCounter = 0; 
double currentCounter = 0;



enum State_enum {WAITING, SET_WEIGHT, GRIND, TRAILING_GRIND};
enum Button_enum {NONE, BUTTON_UP, BUTTON_DOWN, BUTTON_GRIND};

Button_enum buttons = NONE;

State_enum state = WAITING;

// Method forward declarations
void setup();
void machine_state_void();
void update_machine_state();
void run_machine();
void display_all_on();
void OLED_off();
void countdown_grind_time();
void button_management();
void loop();
void get_weight();


void setup() {

  // initialize serial communication:
  Serial.begin(57600);
  Serial.println("------------- setup ----------------------");

  //snprintf(display_buffer, 20, "Previous set weight %4.1f",  previous_set_grind_weight);
  //Serial.println(display_buffer);

  // check if grind time is already written in EEPROM
  //EEPROM.get(eeAddress, set_grind_weight_eeprom);
  // Serial.println(set_grind_weight_eeprom);

  // snprintf(display_buffer, 20, "float %f\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  
  // snprintf(display_buffer, 20, "HEX %x\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  // snprintf(display_buffer, 20, "int %i\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  // snprintf(display_buffer, 20, "string %s\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  // snprintf(display_buffer, 20, "double %d\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  // snprintf(display_buffer, 20, "char %c\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  // snprintf(display_buffer, 20, "unit %u\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  
  // snprintf(display_buffer, 20, "signed floating point %a\n",  set_grind_weight_eeprom);
  // Serial.println(display_buffer);
  // Serial.println("it didnt work did it?");

  // if (set_grind_weight_eeprom == 0){
  //   Serial.print("set grind weight in eeprom, was 0. Setting to ");
  //   Serial.println(set_grind_weight);
  //   //eeprom.put(eeAddress, set_grind_weight);
  // }

 // check if grind time is already written in EEPROM
  //eeprom.get(eeAddress, set_grind_weight_eeprom);
  
  Serial.print("Set grind weight: ");
  Serial.println(set_grind_weight_eeprom);

  pinMode(upButtonPin, INPUT_PULLUP);
  pinMode(downButtonPin, INPUT_PULLUP);
  pinMode(grindButtonPin, INPUT_PULLUP);
  
  pinMode(grindActivatePin, OUTPUT);

  //Get the float data from the EEPROM at position 'eeAddress'
  //eeprom.get(eeAddress, set_grind_weight_eeprom);
  if(debug){Serial.println(set_grind_weight);}  //This may print 'ovf, nan' if the data inside the EEPROM is not a valid float.

  //buttons = NONE;
  Serial.print("button state ");
  Serial.println(buttons);

  //state = WAITING;
  Serial.print("machine state ");
  Serial.println(state);

  // if (debug){
  //   snprintf(display_buffer, 20, "State %i",  state);
  //   Serial.println(display_buffer);
  // }

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display_off();

  Serial.println("Setup finished");
}



void OLED_off(){
  // TOOD, turn OLED off goes here
  Serial.println("turn display off");
}


void get_weight(){

  // TODO: put in reading of scales here
  current_weight ++;

}

void machine_state_void(){

  Serial.println("entered machine state void");
  //strcpy(OLED_msg, "Wating");
  button_management();
  Serial.println("finished button managment");
  Serial.println("button is: ");
  Serial.println(buttons);

  switch (state) {

    case WAITING:
      Serial.println("Waiting state");

      if(buttons == BUTTON_DOWN || buttons == BUTTON_UP){
        if(debug){Serial.println("enter set time mode");}
        //OLED_display("set_grind_weight");
        state = SET_WEIGHT;
      }
      else if(buttons == BUTTON_GRIND){
        if(debug){Serial.println("enter grind mode from waiting");}
        //OLED_display("enter grind");
        state = GRIND;          
      }

      break;
      
    case SET_WEIGHT:
      Serial.print("Set weight state");
      
      if(buttons == BUTTON_GRIND){
        if(debug){Serial.println("enter grind mode from set time");}
        state = GRIND;
      }

      break;

    case GRIND:
      Serial.println("grind state");
      break;

    case TRAILING_GRIND:
      Serial.println("trail grind state");
      break;
  }
}


void run_machine(){

  if (state == GRIND){

    if(debug){
      Serial.println("Running Grind state");

      //sprintf(display_buffer, "Grind set %3.1f",  set_grind_weight);
      //Serial.println(display_buffer);
      
      //sprintf(display_buffer, "current weight %3.1f",  current_weight);
      //Serial.println(display_buffer);
      }

    digitalWrite(grindActivatePin, HIGH);

    dtostrf(current_weight, 4, 2, str_number);
    sprintf(text_buffer, "G - %s - ", str_number);  
    drawCharArry(text_buffer);
    
    get_weight();
 
    Serial.print("after updating weights: ");
    Serial.println(current_weight);

    if (current_weight >= set_grind_weight) {
      Serial.println("reached set weight");
      digitalWrite(grindActivatePin, LOW);
      state = TRAILING_GRIND;
      time_grind_finished = millis();
    }  
  }

   if (state == TRAILING_GRIND){
     // display final weight for a bit and then turn off display unit

     //OLED_display("current_weight");

      if ((millis() - time_grind_finished) >= display_off_delay){
        current_weight = 0;
        display_off();
        state = WAITING;
      
        Serial.print("exited trailing after: ");
        Serial.print((millis() - time_grind_finished)/1000);
        Serial.println("seconds.");
      } else {
        Serial.print("In trailing mode, time left: ");
        Serial.println((display_off_delay - (millis()-time_grind_finished)) / 1000);
      }
    }

  if (state == SET_WEIGHT){
    // TO DO: TURN INDICATOR LIGHTS ON when entering set mode
    // TO DO: turn off lights when returning to wait mode

    //previous_set_grind_weight = set_grind_weight;

    //snprintf(display_buffer, 100, "Previous set weight %4.1f",  previous_set_grind_weight);
    //Serial.println(display_buffer);
    //snprintf(display_buffer, 100, "New set weight %4.1f",  set_grind_weight);
    //Serial.println(display_buffer);

    if (buttons == BUTTON_UP){

      set_grind_weight = set_grind_weight + set_weight_incriment;            
      time_last_setup_b_press = millis();

      if(set_grind_weight > max_grind_weight){
        set_grind_weight = max_grind_weight;
      }
    } else if(buttons == BUTTON_DOWN){

      set_grind_weight = set_grind_weight - set_weight_incriment;            
      time_last_setup_b_press = millis();

      if(set_grind_weight < min_grind_weight){
        set_grind_weight = min_grind_weight;
      }
    }

    Serial.print("set weight previous: ");
    Serial.println(previous_set_grind_weight);
    Serial.println("new set weight: ");
    Serial.println(set_grind_weight);    

    dtostrf(set_grind_weight, 4, 2, str_number);
    sprintf(text_buffer, "W - %s - ", str_number);  
    drawCharArry(text_buffer);
    
    if((millis() - time_last_setup_b_press) > time_exit_setup_after_last_presss){

      Serial.println("exiting set weight mode");
      Serial.println("write set grind weight to EEPROM");
      //eeprom.put(eeAddress, set_grind_weight);
      state = WAITING;
      display_off();
    } else {
        Serial.print("In set weight mode, time left: ");
        Serial.println((time_exit_setup_after_last_presss - (millis() - time_last_setup_b_press)) / 1000);
    }
  }
}






void countdown_grind_time(){

   // Checking whether time is up or not
  if(current_weight <= 0.0){
    digitalWrite(grindActivatePin, LOW);
    OLED_off();
    state = WAITING;
    if (debug){Serial.println("grind finished");}
   }
  else {
    if (debug){
      Serial.print(state);
      Serial.print("Grinding with: ");
    }

    // check if "interval" time has passed (100 econds)
    if ((unsigned long)(currentCounter - previousCounter) >= interval) {
      previousCounter = currentCounter;
      currentCounter = 0;
      //OLED_display("current_weight");
    }  
  }
  
}


void button_management(){
   
  // --------------- detect Down button push -------------
  delay(25);
  
  downButtonPressed = false;
  downButtonState = digitalRead(downButtonPin);
  if(downButtonState != downButtonPrevState){
    downButtonPressed = downButtonState == LOW;
    downButtonPrevState = downButtonState;
  }

  delay(50);
  
  // ----------- detect Up button push -------------
  upButtonPressed = false;
  upButtonState = digitalRead(upButtonPin);
  if(upButtonState != upButtonPrevState){
    upButtonPressed = upButtonState == LOW;
    upButtonPrevState = upButtonState;
  }
  delay(35);

  // ------------ detect Grind button push ------------
  grindButtonPressed = false;
  grindButtonState = digitalRead(grindButtonPin);

  if(grindButtonState != grindButtonPrevState){
    grindButtonPressed = grindButtonState == LOW;
    grindButtonPrevState = grindButtonState;
  }
  
  // ----------- set button state --------------

  // as default buttons is NONE
  buttons = NONE;

  if(downButtonPressed && !upButtonPressed){ 
    if(debug){Serial.println("down");}
    buttons = BUTTON_DOWN;
  }

 if(upButtonPressed && !downButtonPressed){
    if(debug){Serial.println("up");}
    buttons = BUTTON_UP;
  }

 if(grindButtonPressed){
    if(debug){Serial.println("grind button pressed");}
    buttons = BUTTON_GRIND;
  }
}


void loop() {
  
  Serial.println("looping...");
  machine_state_void();
  run_machine();

  delay(100);
  
  

}