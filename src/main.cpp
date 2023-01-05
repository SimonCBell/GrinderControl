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

char serial_print_string[80];
char str_number[20];
const unsigned long display_off_delay = 8 * 1000L; // time after grind is finished that display stays active
unsigned long start_final_display;

const unsigned long safety_max_grind_time = 35 * 1000L; // safety mechanism to avoid grinder running forever
unsigned long start_grind_time;
unsigned long current_grind_time;
unsigned long grind_time_duration;

// WARNING too many serial prints kill OLED initalisation WARNING
bool debug_low = 0;  // debug prints that print each loop (lots!!!)
bool debug_high = 0; // debug prints that print when a state changes or a button is pressed (not sooo many)
bool debug_must = 0; // debug prints that print when a state changes or a button is pressed (not sooo many)

unsigned long last_print_time = 0L;
const unsigned long print_delay = 1 * 1000L;

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

int eeAddress = 0; // EEPROM address to start reading from
float eeprom_storeage_variable;
float set_grind_weight = 19.5; // Variable to store data read from EEPROM.
float previous_set_grind_weight = set_grind_weight;
float slow_grind_state_delta_weight = 1.5; // fast grind mode stops at set_grind_weight - slow_grind_mode_weight

float current_weight = 0.0;
float set_weight_incriment = 0.1;
float max_grind_weight = 100.0;
float min_grind_weight = 2.0;
unsigned long time_last_setup_b_press;
unsigned long time_exit_setup_after_last_presss = 5.0 * 1000L;

// Scaless: HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 3;
const int LOADCELL_SCK_PIN = 2;
int N; // number of scale readings to average

HX711 scale;

enum State_enum
{
  WAITING,
  SET_WEIGHT,
  INITALISE_GRIND,
  GRIND_FAST,
  GRIND_SLOW,
  FINAL_DISPLAY,
  SAFETY_STOP_GRIND
};

enum Button_enum
{
  NONE,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_GRIND
};

Button_enum buttons = NONE;

State_enum state = WAITING;

// Method forward declarations
void setup();
void machine_state_void();
void run_machine();
void button_management();
void loop();
void slow_serial_println(char serial_print_string[80]);

void setup()
{

  // initialize serial communication:
  Serial.begin(57600);
  Serial.println("------------- setup ----------------------");

  // ----------------- setup OLED ------------------------
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  display_off();

  // -------- define set grind weight--------------
  // check if set grind weight is already written in EEPROM
  // otherwise take default and write it in EEPROM
  EEPROM.get(eeAddress, eeprom_storeage_variable);

  if (eeprom_storeage_variable <= 0 || isnan(eeprom_storeage_variable))
  {
    Serial.print("Set grind weight in EEPROM was invalid. Write set grind weight in eeprom to default: ");
    Serial.println(set_grind_weight);
    EEPROM.put(eeAddress, set_grind_weight);
  }
  else
  {
    Serial.print("Set grind weight from EEPROM is valid: ");
    Serial.print(eeprom_storeage_variable);
    Serial.println(" g"); // if set grind weight in EEPROM is valid, write it to set grind weight
    set_grind_weight = eeprom_storeage_variable;
    previous_set_grind_weight = set_grind_weight;
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
  scale.set_scale(-2177.5); // this value is obtained by calibrating the scale with known weights; see the README for details

  scale.power_down(); // put the ADC in sleep mode
  delay(100);

  Serial.println("Setup finished");
}

void tare_scales()
{
  scale.power_up();
  scale.tare(); // reset the scale to 0
}

void shutdown_scales()
{
  scale.power_down();
}

void get_weight(int N)
{
  current_weight = scale.get_units(N);
}

void slow_serial_println(char serial_print_string[80])
{
  if (millis() - last_print_time >= print_delay)
  {
  Serial.println(serial_print_string);
  last_print_time = millis();
  }
}

void machine_state_void()
{

  // update button state
  button_management();

  if (debug_low)
  {
    Serial.println("entered machine state void");
    Serial.println("finished button managment");
    Serial.println("button is: ");
    Serial.println(buttons);
  }

  switch (state)
  {

  case WAITING:
    if (debug_low)
    {
      Serial.println("Waiting state");
    }

    if (buttons == BUTTON_DOWN || buttons == BUTTON_UP)
    {
      if (debug_must)
      {
        Serial.println("enter set weight state from waiting");
      }
      state = SET_WEIGHT;
    }
    else if (buttons == BUTTON_GRIND)
    {
      current_weight = 0;
      if (debug_must)
      {
        Serial.println("enter grind state from waiting");
      }
      state = INITALISE_GRIND;
    }
    break;

  case SET_WEIGHT:
    if (debug_low)
    {
      Serial.print("Set weight state");
    }

    if (buttons == BUTTON_GRIND)
    {
      current_weight = 0;
      if (debug_must)
      {
        Serial.println("enter grind state from set time");
      }
      state = INITALISE_GRIND;
    }
    break;

  case INITALISE_GRIND:
    if (debug_low)
    {
      Serial.println("initalise grind state");
    }
    break;

  case GRIND_FAST:
    if (debug_low)
    {
      Serial.println("grind fast state");
    }
    break;

  case GRIND_SLOW:
    if (debug_low)
    {
      Serial.println("grind slow state");
    }
    break;

  case FINAL_DISPLAY:
    if (debug_low)
    {
      Serial.println("final display state");
    }

    if (buttons == BUTTON_DOWN || buttons == BUTTON_UP)
    {
      if (debug_must)
      {
        Serial.println("enter set weight mode from trailing");
      }
      state = SET_WEIGHT;
    }
    else if (buttons == BUTTON_GRIND)
    {
      current_weight = 0;
      if (debug_must)
      {
        Serial.println("enter grind state from waiting");
      }
      state = INITALISE_GRIND;
    }
    break;

  case SAFETY_STOP_GRIND:
    if (debug_low)
    {
      Serial.println("safety stop grind state");
    }
    break;
  }
}

void run_machine()
{

  if (state == INITALISE_GRIND)
  {
    if (debug_low)
    {
      Serial.println("Running intialise grind state");
    }

    tare_scales();
    start_grind_time = millis();
    state = GRIND_FAST;
  }

  if (state == GRIND_FAST)
  {
    if (debug_low)
    {
      Serial.println("Running fast grind state");
    }

    //TODO : is this previous = set needed ? ? ?
    //previous_set_grind_weight = set_grind_weight;

    // if max grind time exceeded, enter safety stop grind state
    current_grind_time = millis();
    grind_time_duration = current_grind_time - start_grind_time;
    if (grind_time_duration >= safety_max_grind_time)
    {
      if (debug_must)
      {
        Serial.println("saftey grind time exceeded from fast grind");
      }
      state = SAFETY_STOP_GRIND;
    }
    else
    {
      if (debug_high)
      {
        snprintf (serial_print_string, sizeof(serial_print_string), "grind time duration in fast grind: %lu", grind_time_duration);
        slow_serial_println(serial_print_string);
      }
    }
  

    digitalWrite(grindActivatePin, HIGH);

    // read scales without averaging, for fast response
    get_weight(1);

    if (debug_low)
    {
      //TODO: convert to slow_sertial_print
      Serial.print("after updating weights: ");
      Serial.println(current_weight);
    }
    drawWeightScreen(current_weight);

    if (debug_low)
    {
      //TODO: convert to slow_sertial_print
      Serial.println(set_grind_weight - slow_grind_state_delta_weight);
    }

    if (current_weight >= (set_grind_weight - slow_grind_state_delta_weight))
    {
      digitalWrite(grindActivatePin, LOW);
      if (debug_must)
      {
        Serial.println("reached fast grind set weight");
      }
      state = GRIND_SLOW;
    }
    else
    {
      if (debug_low)
      {
        slow_serial_println("slow grind limit not reached yet");
      }
    }
  }

  if (state == GRIND_SLOW)
  {
    if (debug_low)
    {
      Serial.println("Running slow grind state");
    }

    // if max grind time exceeded, enter safety stop grind state
    current_grind_time = millis();
    grind_time_duration = current_grind_time - start_grind_time;
    if (grind_time_duration >= safety_max_grind_time)
    {
      if (debug_must)
      {
        Serial.println("saftey grind time exceeded from slow grind");
      }
      state = SAFETY_STOP_GRIND;
    }
    else
    {
      if (debug_must)
      {
        snprintf (serial_print_string, sizeof(serial_print_string), "grind time duration in slow grind: %lu", grind_time_duration);
        slow_serial_println(serial_print_string);
      }
    }

    digitalWrite(grindActivatePin, HIGH);
    delay(200);
    digitalWrite(grindActivatePin, LOW);
    delay(500);

    // read scales with 10 averages, for more accurate result
    get_weight(10);

    if (debug_must)
    {
      Serial.print("after updating weights: ");
      Serial.println(current_weight);
    }

    drawWeightScreen(current_weight);

    if (current_weight >= set_grind_weight)
    {
      drawWeightScreen(current_weight);

      shutdown_scales();
      start_final_display = millis();
      state = FINAL_DISPLAY;

    }
  }

  if (state == FINAL_DISPLAY)
  {
    // wait to give user chance to read last display message
    // then turn off display unit

    if ((millis() - start_final_display) >= display_off_delay)
    {
      display_off();
      state = WAITING;
      if (debug_high)
      {
        Serial.print("exited final disaplay state after: ");
        Serial.print((millis() - start_final_display) / 1000);
        Serial.println("seconds.");
      }
    }
    else
    {
      //drawWeightScreen(current_weight);
      if (debug_high)
      {
        snprintf (serial_print_string, sizeof(serial_print_string), "In final display state, time left: %lu", (display_off_delay - (millis() - start_final_display)) / 1000);
        slow_serial_println(serial_print_string); 
      }
    }
  }

  if (state == SAFETY_STOP_GRIND)
  {
    // safety mechanism that aborts grinding to avoid grind running forever
    // triggered if max grind time was exceeded

    if (debug_must)
    {
      Serial.println("safety time exceeded, aborting grind"); 
    }

    digitalWrite(grindActivatePin, LOW);
    shutdown_scales();

    drawAbortText();
    start_final_display = millis();
    state = FINAL_DISPLAY;
  }

  if (state == SET_WEIGHT)
  {

    if (buttons == BUTTON_UP)
    {

      set_grind_weight = set_grind_weight + set_weight_incriment;
      time_last_setup_b_press = millis();

      if (set_grind_weight > max_grind_weight)
      {
        set_grind_weight = max_grind_weight;
      }
    }
    else if (buttons == BUTTON_DOWN)
    {

      set_grind_weight = set_grind_weight - set_weight_incriment;
      time_last_setup_b_press = millis();

      if (set_grind_weight < min_grind_weight)
      {
        set_grind_weight = min_grind_weight;
      }
    }

    if (debug_high)
    {
      snprintf (serial_print_string, sizeof(serial_print_string), "set weight previous: %f \n new set weight: %f ", previous_set_grind_weight, set_grind_weight);
      slow_serial_println(serial_print_string);
    }

    drawSetWeightScreen(previous_set_grind_weight, set_grind_weight);

    if ((millis() - time_last_setup_b_press) > time_exit_setup_after_last_presss)
    {

      previous_set_grind_weight = set_grind_weight;
      if (debug_must)
      {
        slow_serial_println("exiting set weight mode write set grind weight to EEPROM");
      }
      EEPROM.put(eeAddress, set_grind_weight);
      state = WAITING;
      display_off();
    }
    else
    {
      if (debug_high)
      {
        snprintf (serial_print_string, sizeof(serial_print_string), "In set weight state, time left: %lu", (time_exit_setup_after_last_presss - (millis() - time_last_setup_b_press)) / 1000);
        slow_serial_println(serial_print_string);
      }
    }
  }
}

void button_management()
{

  // --------------- detect Down button push -------------
  delay(10);

  downButtonPressed = false;
  downButtonState = digitalRead(downButtonPin);
  if (downButtonState != downButtonPrevState)
  {
    downButtonPressed = downButtonState == LOW;
    downButtonPrevState = downButtonState;
  }

  // ----------- detect Up button push -------------
  upButtonPressed = false;
  upButtonState = digitalRead(upButtonPin);
  if (upButtonState != upButtonPrevState)
  {
    upButtonPressed = upButtonState == LOW;
    upButtonPrevState = upButtonState;
  }

  // ------------ detect Grind button push ------------
  grindButtonPressed = false;
  grindButtonState = digitalRead(grindButtonPin);

  if (grindButtonState != grindButtonPrevState)
  {
    grindButtonPressed = grindButtonState == LOW;
    grindButtonPrevState = grindButtonState;
  }

  // ----------- set button state --------------

  // as default buttons is NONE
  buttons = NONE;

  if (downButtonPressed && !upButtonPressed)
  {
    if (debug_high)
    {
      Serial.println("down");
    }
    buttons = BUTTON_DOWN;
  }

  if (upButtonPressed && !downButtonPressed)
  {
    if (debug_high)
    {
      Serial.println("up");
    }
    buttons = BUTTON_UP;
  }

  if (grindButtonPressed)
  {
    if (debug_high)
    {
      Serial.println("grind button pressed");
    }
    buttons = BUTTON_GRIND;
  }
}

void loop()
{
  machine_state_void();
  run_machine();
  delay(25);
}