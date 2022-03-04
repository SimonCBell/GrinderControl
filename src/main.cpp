#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Display settings
uint8_t oled_i2c = 0x3C;
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
char* display_buffer;
unsigned long display_off_delay = 8 * 1000; // time after grind is finished that display stays active
unsigned long time_grind_finished = 0;


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
float set_grind_weight_eeprom;

float current_weight = 0;
float set_weight_incriment = 0.1;
float max_grind_weight = 100;
float min_grind_weight = 2;


int secs;
int tenths_secs;

double interval = 3;
double previousCounter = 0; 
double currentCounter = 0;

int setup_time_counter = 0;
int setup_time_limit = 200;

enum State_enum {WAITING, SET_WEIGHT, GRIND, TRAILING_GRIND};
enum Button_enum {NONE, BUTTON_UP, BUTTON_DOWN, BUTTON_GRIND};

uint8_t button_management();
uint8_t buttons;

uint8_t state = WAITING;

// Method forward declarations
void setup();
void machine_state_void();
void run_machine();
void display_all_on();
void OLED_off();
void OLED_display(float msg_num);
void countdown_grind_time();
uint8_t button_management();
void loop();
void get_weight();


void setup() {

  // initialize serial communication:
  if(debug){
    Serial.begin(115200);
    Serial.println("------------- setup ----------------------");
  }

  // check if grind time is already written in EEPROM
  set_grind_weight_eeprom = EEPROM.get(eeAddress, set_grind_weight);
  Serial.print("set grind weight in eeprom ");
  Serial.println(set_grind_weight_eeprom);
  if (set_grind_weight_eeprom == 0){
    Serial.print("set grind weight in eeprom, was 0. Setting to ");
    Serial.println(set_grind_weight);
    EEPROM.put(eeAddress, set_grind_weight);
  }

  pinMode(upButtonPin, INPUT_PULLUP);
  pinMode(downButtonPin, INPUT_PULLUP);
  pinMode(grindButtonPin, INPUT_PULLUP);
  
  pinMode(grindActivatePin, OUTPUT);


  //Get the float data from the EEPROM at position 'eeAddress'
  EEPROM.get(eeAddress, set_grind_weight);
  if(debug){Serial.println(set_grind_weight);}  //This may print 'ovf, nan' if the data inside the EEPROM is not a valid float.

  //TODO: why does this crash the szstem!!
  //u8g2.setI2CAddress(oled_i2c * 2);
  //u8g2.begin();

  if (debug){Serial.println(state);}
  
  //u8g2.clearBuffer();					// clear the internal memory
  //u8g2.setFont(u8g2_font_ncenB08_tr);	// choose a suitable font
  //u8g2.drawStr(0, 10, "Display init");	// write something to the internal memory
  //u8g2.sendBuffer();					// transfer internal memory to the display

}


void OLED_display(float msg_num){
  // to do OLED printing goes here
  Serial.print(msg_num);
  Serial.println("display requst");
}

void OLED_off(){
  // TOOD, turn OLED off goes here
  Serial.println("turn display off");
}


void get_weight(){

  // TODO: put in reading of scales here
  current_weight ++;

}

void machine_state_void()
 {
   switch(state)
   {
      case WAITING:
        buttons = button_management();
      
        if(buttons == BUTTON_DOWN || buttons == BUTTON_UP){
          if(debug){Serial.println("enter set time mode");}
          OLED_display(set_grind_weight);
          state = SET_WEIGHT;
        }
        else if(buttons == BUTTON_GRIND){
          if(debug){Serial.println("enter grind mode from waiting");}
          state = GRIND;
        }

        break;
        
      case SET_WEIGHT:
        buttons = button_management();
        
        if(buttons == BUTTON_GRIND){
          if(debug){Serial.println("enter grind mode from set time");}
          state = GRIND;
        }
        
        setup_time_counter++;

        break;

      case GRIND:
        break;

      case TRAILING_GRIND:
      break;
   }
 }

void run_machine(){

   if (state == GRIND){

    if(debug){
      Serial.println("Running Grind state");
      Serial.print("grind set" );
      Serial.println(set_grind_weight);
      Serial.print("current weight");
      Serial.println(current_weight);
      }

    digitalWrite(grindActivatePin, HIGH);

    //  OLED_display(current_weight);
    
    get_weight();
 
    Serial.print("after updating weights");
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

     OLED_display(current_weight);

     Serial.println("entered trailing mode");
     Serial.println(millis() - time_grind_finished);
     Serial.println(display_off_delay);

     if ((millis() - time_grind_finished) >= display_off_delay){
       current_weight = 0;
       OLED_off();
       state = WAITING;
     }
   }

  if (state == SET_WEIGHT){
    // TO DO: TURN INDICATOR LIGHTS ON when entering set mode
    // TO DO: turn off lights when returning to wait mode

    //previous_set_grind_weight = set_grind_weight;

    Serial.println("set weight mode ");
    Serial.println("previous set weight");
    Serial.println(previous_set_grind_weight);
    Serial.println("new set weight ");
    Serial.println(set_grind_weight);
        
    if(buttons == BUTTON_UP){

      set_grind_weight = set_grind_weight + set_weight_incriment;            
      setup_time_counter = 0;

      if(set_grind_weight > max_grind_weight){
        set_grind_weight = max_grind_weight;
      }

      OLED_display(set_grind_weight);

    } else if(buttons == BUTTON_DOWN){

      set_grind_weight = set_grind_weight - set_weight_incriment;            
      setup_time_counter = 0;

      if(set_grind_weight < min_grind_weight){
        set_grind_weight = min_grind_weight;
      }

      OLED_display(set_grind_weight);

    } else if(setup_time_counter > setup_time_limit){

      Serial.println("write set grind weight to EEPROM");
      EEPROM.put(eeAddress, set_grind_weight);
      state = WAITING;
      OLED_off();
      setup_time_counter = 0;
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
  else
  {

    
    if (debug){
      Serial.print(state);
      Serial.print("Grinding with: ");
    }

 
    // check if "interval" time has passed (100 econds)
    if ((unsigned long)(currentCounter - previousCounter) >= interval) {
       previousCounter = currentCounter;
       currentCounter = 0;
       OLED_display(current_weight);
       }  
  }
  
}


uint8_t button_management(){
   
  enum Button_enum Button_action;

  Button_action = NONE;
   
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
  
  // ----------- set button action --------------

  if(downButtonPressed && !upButtonPressed){ 
    if(debug){Serial.println("down");}
    Button_action = BUTTON_DOWN;
  }

 if(upButtonPressed && !downButtonPressed){
    if(debug){Serial.println("up");}
    Button_action = BUTTON_UP;
  }

 if(grindButtonPressed){
    if(debug){Serial.println("grind button pressed");}
    Button_action = BUTTON_GRIND;
  }

  return Button_action;    
}


void loop() {
  // put your main code here, to run repeatedly:
 
 machine_state_void();
 run_machine();
 delay(200);


}