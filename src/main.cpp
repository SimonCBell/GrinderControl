#include <Arduino.h>
#include <EEPROM.h>
#include "display.h"
#include "HX711.h"

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


bool debug_low = 0;  // debug prints that print each loop (lots!!!)
bool debug_high = 0; // debug prints that print when a state changes or a button is pressed (not sooo many)
const int downButtonPin = 12;
const int upButtonPin = 11;
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
float eeprom_storeage_variable;
float set_grind_weight = 19.5; //Variable to store data read from EEPROM.
float previous_set_grind_weight = set_grind_weight;
float slow_grind_mode_weight = 1.5; // fast grind mode stops at set_grind_weight - slow_grind_mode_weight

float current_weight = 0.0;
float set_weight_incriment = 0.1;
float max_grind_weight = 100.0;
float min_grind_weight = 2.0;
unsigned long time_last_setup_b_press;
unsigned long time_exit_setup_after_last_presss = 5.0 * 1000;

// Scaless: HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 3;
const int LOADCELL_SCK_PIN = 2;
int N; //number of scale readings to average

HX711 scale;

enum State_enum {WAITING, SET_WEIGHT, TARE_SCALES, GRIND_FAST, GRIND_SLOW, TRAILING_GRIND};
enum Button_enum {NONE, BUTTON_UP, BUTTON_DOWN, BUTTON_GRIND};

Button_enum buttons = NONE;

State_enum state = WAITING;

// Method forward declarations
void setup();
void machine_state_void();
void update_machine_state();
void run_machine();
void display_all_on();
void countdown_grind_time();
void button_management();
void loop();
void get_weight();


void setup() {

  // initialize serial communication:
  Serial.begin(57600);
  Serial.println("------------- setup ----------------------");

  // ----------------- setup OLED ------------------------
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display_off();

  // -------- define set grind weight--------------
  // check if set grind weight is already written in EEPROM
  // otherwise take default and write it in EEPROM 
  EEPROM.get(eeAddress, eeprom_storeage_variable);
  
  if (eeprom_storeage_variable <= 0 || isnan(eeprom_storeage_variable)){
    Serial.print("Set grind weight in EEPROM was invalid. Write set grind weight in eeprom to default: ");
    Serial.println(set_grind_weight);
    EEPROM.put(eeAddress, set_grind_weight);

  } else{
    Serial.print("Set grind weight from EEPROM is valid: ");
    Serial.print(eeprom_storeage_variable);
    Serial.println(" g");     // if set grind weight in EEPROM is valid, write it to set grind weight
    set_grind_weight = eeprom_storeage_variable;
  }

  // ------------ define pins ---------------
  pinMode(upButtonPin, INPUT_PULLUP);
  pinMode(downButtonPin, INPUT_PULLUP);
  pinMode(grindButtonPin, INPUT_PULLUP);
  
  pinMode(grindActivatePin, OUTPUT);

  // ----------- initalise button and machine states
  buttons = NONE;
  Serial.print("button state ");
  Serial.println(buttons);

  state = WAITING;
  Serial.print("machine state ");
  Serial.println(state);




  // ------------setup scales ---------------------
  Serial.println("HX711 Demo");
  Serial.println("Initializing the scale");
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(-2177.5);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  
  scale.power_down();			        // put the ADC in sleep mode
  delay(100);

  Serial.println("Setup finished");
}

void tare_scales(){
  scale.power_up();
  scale.tare();				        // reset the scale to 0
}

void shutdown_scales(){
    scale.power_down();
}

void get_weight(int N){
  current_weight = scale.get_units(N);
}

void machine_state_void(){

  // update button state
  button_management();

  if (debug_low){
    Serial.println("entered machine state void");
    Serial.println("finished button managment");
    Serial.println("button is: ");
    Serial.println(buttons);
  }

  switch (state) {

    case WAITING:
      if (debug_low){ Serial.println("Waiting state");}

      if(buttons == BUTTON_DOWN || buttons == BUTTON_UP){
        if(debug_high){Serial.println("enter set time mode");}
        state = SET_WEIGHT;
      }
      else if(buttons == BUTTON_GRIND){
        current_weight = 0;
        if(debug_high){Serial.println("enter grind mode from waiting");}
        state = TARE_SCALES;          
      }
      break;
      
    case SET_WEIGHT:
      if (debug_high){Serial.print("Set weight state");}
      
      if(buttons == BUTTON_GRIND){
        current_weight = 0;
        if(debug_high){Serial.println("enter grind mode from set time");}
        state = TARE_SCALES;
      }
      break;

    case GRIND_FAST:
      if (debug_high){Serial.println("grind fast state");}
      break;

    case GRIND_SLOW:
      if (debug_high){Serial.println("grind slow state");}
      break;

    case TARE_SCALES:
    if (debug_high){Serial.println("Tare scales state");}
      break;

    case TRAILING_GRIND:
    if (debug_high){Serial.println("trail grind state");}
      
      if(buttons == BUTTON_DOWN || buttons == BUTTON_UP){
        if(debug_high){Serial.println("enter set time mode");}
        state = SET_WEIGHT;
      }
      else if(buttons == BUTTON_GRIND){
        current_weight = 0;
        if(debug_high){Serial.println("enter grind mode from waiting");}
        state = TARE_SCALES;          
      }
      break;
  }
}


void run_machine(){

  if (state == TARE_SCALES){
    tare_scales();
    state = GRIND_FAST;
  }

  if (state == GRIND_FAST){

    previous_set_grind_weight = set_grind_weight;

    if(debug_low){Serial.println("Running Grind state");}

    digitalWrite(grindActivatePin, HIGH);
    
    // read scales without averaging, for fast response
    get_weight(1);

    if (debug_low){
      Serial.print("after updating weights: ");
      Serial.println(current_weight);
    }
    drawWeightScreen(current_weight);

    if (debug_high) {Serial.println(set_grind_weight - slow_grind_mode_weight);}

    if (current_weight >= (set_grind_weight - slow_grind_mode_weight)) {
      digitalWrite(grindActivatePin, LOW);
      if (debug_high) {Serial.println("reached fast grind set weight");}
      state = GRIND_SLOW;
    }  else{
      if (debug_high) {Serial.println("slow grind limit not reached yet");}
    }
  }

  if (state == GRIND_SLOW){

    digitalWrite(grindActivatePin, HIGH);
    delay(200);
    digitalWrite(grindActivatePin, LOW);
    delay(500);

    // read scales with 10 averages, for more accurate result
    get_weight(10);
 
    if (debug_high) {
      Serial.print("after updating weights: ");
      Serial.println(current_weight);
    }

    drawWeightScreen(current_weight);

    if (current_weight >= set_grind_weight) {
      if (debug_high) {
        Serial.print("after updating weights: ");
        Serial.println(current_weight);
      }

      drawWeightScreen(current_weight);

      shutdown_scales();
      time_grind_finished = millis();
      state = TRAILING_GRIND;
    }
  }

   if (state == TRAILING_GRIND){
      // display final weight for a bit and then turn off display unit

      if ((millis() - time_grind_finished) >= display_off_delay){
        display_off();
        state = WAITING;
        if (debug_high) {
          Serial.print("exited trailing after: ");
          Serial.print((millis() - time_grind_finished)/1000);
          Serial.println("seconds.");
        }
      } else {
        drawWeightScreen(current_weight);
        if (debug_high) {
        Serial.print("In trailing mode, time left: ");
        Serial.println((display_off_delay - (millis()-time_grind_finished)) / 1000);
        }
      }
    }

  if (state == SET_WEIGHT){

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
    
    if (debug_high) {
      Serial.print("set weight previous: ");
      Serial.println(previous_set_grind_weight);
      Serial.println("new set weight: ");
      Serial.println(set_grind_weight);    
    }

    drawSetWeightScreen(previous_set_grind_weight, set_grind_weight);

    if((millis() - time_last_setup_b_press) > time_exit_setup_after_last_presss){
      
      previous_set_grind_weight = set_grind_weight;
      if (debug_high) {
        Serial.println("exiting set weight mode");
        Serial.println("write set grind weight to EEPROM");
      }
      EEPROM.put(eeAddress, set_grind_weight);
      state = WAITING;
      display_off();
    } else {
        if (debug_high) {
          Serial.print("In set weight mode, time left: ");
          Serial.println((time_exit_setup_after_last_presss - (millis() - time_last_setup_b_press)) / 1000);
        }
    }
  }
}


void countdown_grind_time(){

   // Checking whether time is up or not
  if(current_weight <= 0.0){
    digitalWrite(grindActivatePin, LOW);
    display_off();
    state = WAITING;
    if (debug_high){Serial.println("grind finished");}
   }
  else {
    if (debug_low){
      Serial.print(state);
      Serial.print("Grinding with: ");
    }
  }
}


void button_management(){
   
  // --------------- detect Down button push -------------
  delay(10);
  
  downButtonPressed = false;
  downButtonState = digitalRead(downButtonPin);
  if(downButtonState != downButtonPrevState){
    downButtonPressed = downButtonState == LOW;
    downButtonPrevState = downButtonState;
  }
  
  // ----------- detect Up button push -------------
  upButtonPressed = false;
  upButtonState = digitalRead(upButtonPin);
  if(upButtonState != upButtonPrevState){
    upButtonPressed = upButtonState == LOW;
    upButtonPrevState = upButtonState;
  }

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
    if(debug_high){Serial.println("down");}
    buttons = BUTTON_DOWN;
  }

 if(upButtonPressed && !downButtonPressed){
    if(debug_high){Serial.println("up");}
    buttons = BUTTON_UP;
  }

 if(grindButtonPressed){
    if(debug_high){Serial.println("grind button pressed");}
    buttons = BUTTON_GRIND;
  }
}


void loop() {
  machine_state_void();
  run_machine();
  delay(25);
}