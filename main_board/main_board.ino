#include <Arduino.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ClickButton.h>

// CONSTANTS
const bool NA = HIGH;
const bool NF = LOW;
const unsigned long filteringCycleStartTime = 0;
const unsigned long filteringCycleStopTime = 21600000; // 6 hours
const unsigned long millisInADay = 86400000; // 24 hours
const int Contrast=15;
const int Brightnss = 48;
const int actionState_standby = 0;
const int actionState_filtration= 1;
const int actionState_heating = 2;
const int menuState_main = 0;
const int menuState_heating = 1;
const int menuState_filtering = 2;
const unsigned long defaultCheckTemperatureInterval = 30000;
const unsigned long defaultUpdateLCDInterval = 500;
const int buttonsAmount = 3;
const int button_set = 0;
const int button_down = 1;
const int button_up = 2;

// PINS
const int pin_relay0 = A3; // motor - velocity 0
const int pin_relay1 = A4; // motor - velocity 1
const int pin_relay2 = A5; // motor - velocity 2
const int pin_relay3 = 8; // compressor
const int pin_relay4 = 7; // water bomb
const int pin_lcd_data0 = 2; // data_0
const int pin_lcd_data1 = 3; // data_1
const int pin_lcd_data2 = 4; // data_2
const int pin_lcd_data3 = 5; // data_3
const int pin_lcd_RS = 12; // RS (Register Select)
const int pin_lcd_enable = 11; // Enable
const int pin_lcd_contrast = 6; // LCD contrast
const int pin_lcd_brightness = 9; // LCD brightness
const int pin_button_set = A0; // Set button
const int pin_button_down = A1; // Down button
const int pin_button_up = A2; // Up button
const int pin_termometer = 10; // Temperature Sensor

// LCD
LiquidCrystal lcd(pin_lcd_RS, pin_lcd_enable, pin_lcd_data3,
                  pin_lcd_data2, pin_lcd_data1, pin_lcd_data0);

// Temperature sensor
OneWire oneWire(pin_termometer);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1;

// ClickButtons
ClickButton button[3] = {
  ClickButton (pin_button_set, LOW, CLICKBTN_PULLUP),
  ClickButton (pin_button_down, LOW, CLICKBTN_PULLUP),
  ClickButton (pin_button_up, LOW, CLICKBTN_PULLUP),
};

// GLOBAL VARIABLES
unsigned long time = 0;
unsigned long nextTimeCheckTemperature = 0;
unsigned long lastTimeUpdatedLCD = 0;
float water_temperature = 0.0;
int currentActionState = actionState_standby;
int currentMenuState = menuState_main;
int lastButtonPressed = -1;
boolean buttonPressed = false;
boolean forcedFiltering = false; // Forced filtering - ON or OFF
boolean autoFiltering = false; // Auto filtering - ON or OFF
boolean heating = false; // Heating System - ON or OFF
float heatingTemperature = 20;

void setup() {
  // Setup pins
  pinMode(pin_relay0, OUTPUT);
  pinMode(pin_relay1, OUTPUT);
  pinMode(pin_relay2, OUTPUT);
  pinMode(pin_relay3, OUTPUT);
  pinMode(pin_relay4, OUTPUT);
  pinMode(pin_lcd_brightness, OUTPUT);

  // Setup LCD
  lcd.begin(16, 2);
  analogWrite(pin_lcd_contrast,Contrast);
  analogWrite(pin_lcd_brightness, Brightnss);

  // Setup sensors
  sensors.begin();
  sensors.getAddress(sensor1, 0);
  
  // Setup ClickButtons
  for (int i=0; i < buttonsAmount; i++)
  {
    button[i].debounceTime   = 20;   // Debounce timer in ms
    button[i].multiclickTime = 250;  // Time limit for multi clicks
    button[i].longClickTime  = 1000; // Time until long clicks register
  }
}

void loop() {
  // Time since start. Resets back to 0 every 24 hours.
  time = (millis() % millisInADay);
  
  // Update water temperature
  updateWaterTemperature();
  
  // Check whether auto filtering should be turned on or off
  checkAutoFiltering();
  
  // Check whether heating should be turned on or off
  checkHeating();
  
  // Check Button Clicks
  checkButtonClicks();
  
  // Perform button click action
  doButtonClickAction();
  
  // Selects currentActionState
  actionStateSelector();
  
  // Do current action state
  doCurrentActionState();
    
  // Update LCD
  updateLCD();
}

// Checks whether heating should be turned on or off
void checkHeating() {
  // DEBUG - Implement better version. Based on conditionals decide
 // whether heating should be on or off. Read notes.txt.
  if(water_temperature < heatingTemperature) {
    heating = true;
  }
  else {
    heating = false;
  }
}

// Does button click action
void  doButtonClickAction() {
 if(!buttonPressed)
   return;
  
  buttonPressed = false;
  
  // Select click action
  switch(lastButtonPressed) {
    case button_set:
      switch(currentMenuState) {
        case menuState_main:
          currentMenuState = menuState_heating;
        break;
        case menuState_heating:
          currentMenuState = menuState_filtering;
        break;
        case menuState_filtering:
          currentMenuState = menuState_main;
        break;
      }
    break;
    case button_up:
      switch(currentMenuState) {
        case menuState_main:
          // do nothing
        break;
        case menuState_heating:
          heatingTemperature += 1.0;
        break;
        case menuState_filtering:
          forcedFiltering = !forcedFiltering;
        break;
      }
    break;
    case button_down:
      switch(currentMenuState) {
        case menuState_main:
          // do nothing
        break;
        case menuState_heating:
          heatingTemperature -= 1.0;
        break;
        case menuState_filtering:
          forcedFiltering = !forcedFiltering;
        break;
      }
    break;
  } 
}

// Selects action state
void actionStateSelector() {
  currentActionState = actionState_standby;
  // Check if its auto filtration time
  if(autoFiltering)
    currentActionState = actionState_filtration;
  
  // Check if filtration is forced
  if(forcedFiltering == true)
    currentActionState = actionState_filtration;
  
  // Check if heating is on
  if(heating)
    currentActionState = actionState_heating;
}

// Do current action state action
void doCurrentActionState() {
  switch(currentActionState) {
    case actionState_standby:
      // Turn off motor
      switchMotor(0, NF);
      // Turn off compressor
      switchCompressor(NF);
      // Turn of Water Bomb
      switchWaterBomb(NF);
      break;
    case actionState_filtration:
      // Turn on Water Bomb
      switchWaterBomb(NA);
      // Turn off motor
      switchMotor(0, NF);
      // Turn off compressor
      switchCompressor(NF);
      break;
    case actionState_heating:
      // Turn on Water Bomb
      switchWaterBomb(NA);
      // Turn on motor
      switchMotor(0, NA);
      // Turn on compressor
      switchCompressor(NA);
      break;
  }
}

// Detects button clicks
void checkButtonClicks() {
  // Go through each button
  for (int i=0; i<buttonsAmount; i++)
  {
    // Update state of all buittons
    button[i].Update();
    
    // Mark pressed button and leave function
    if(button[i].clicks == 1) {
      switch(i) {
        case button_set:
          buttonPressed = true;
          lastButtonPressed = button_set;
          return;
        break;
        case button_up:
          buttonPressed = true;
          lastButtonPressed = button_up;
          return;
        break;
        case button_down:
          buttonPressed = true;
          lastButtonPressed = button_down;
          return;
        break;
      }
    }
  }
}

// Updates LCD data
void updateLCD() {
  if( (time - lastTimeUpdatedLCD) > defaultUpdateLCDInterval ) {
    lastTimeUpdatedLCD = time;
    lcd.clear();
    
    switch(currentMenuState) {
      case menuState_main:
        lcd.setCursor(4, 0);
        lcd.print(water_temperature, 2);
        lcd.print((char)223);
        lcd.print("C");
        if(currentActionState == actionState_standby){
          lcd.setCursor(2, 1);
          lcd.print("Em descanso");
        }
        else if(currentActionState == actionState_filtration && forcedFiltering == false) {
          lcd.setCursor(3, 1);
          lcd.print("Filt. auto");
        }
        else if(currentActionState == actionState_filtration && forcedFiltering == true) {
          lcd.setCursor(1, 1);
          lcd.print("Filt. forcada");
        }
        else if(currentActionState == actionState_heating) {
          lcd.setCursor(3, 1);
          lcd.print("Aquecendo");
        }
        break;
       case menuState_heating:
        lcd.setCursor(2, 0);
        lcd.print("Aquecer ate");
        lcd.setCursor(5, 1);
        lcd.print(heatingTemperature, 2);
        lcd.print((char)223);
        lcd.print("C");
       break;
       case menuState_filtering:
        lcd.setCursor(0, 0);
        lcd.print("Forcar filtragem");
        if(forcedFiltering){
          lcd.setCursor(5, 1);
          lcd.print("Ligado");
        }
        else{
          lcd.setCursor(3, 1);
          lcd.print("Desligado");
        }
       break;
    }
  }
}

// Switches motors to NA or to NF
// If state = NF, all motors will go OFF.
// If state = NA, only ONE motor will go ON at a time
void switchMotor(int velocity, bool state) {
  switch(velocity) {
    case 0:
      // Sets all other relays to NF
      digitalWrite(pin_relay1, NF);
      digitalWrite(pin_relay2, NF);
      digitalWrite(pin_relay0, state);
      break;
    case 1:
      // Sets all other relays to NF
      digitalWrite(pin_relay0, NF);
      digitalWrite(pin_relay2, NF);
      digitalWrite(pin_relay1, state);
      break;
    case 2:
      // Sets all other relays to NF
      digitalWrite(pin_relay0, NF);
      digitalWrite(pin_relay1, NF);
      digitalWrite(pin_relay2, state);
      break;
  }
}

// Switches compressor to NA or to NF
void switchCompressor(bool state) {
    digitalWrite(pin_relay3, state);
}

// Switches water bomb to NA or to NF
void switchWaterBomb(bool state) {
    digitalWrite(pin_relay4, state);
}

// Updates water temperature reading from sensor
void updateWaterTemperature() {
  if( time > nextTimeCheckTemperature )  {
    sensors.requestTemperatures();
    water_temperature = sensors.getTempC(sensor1);
    nextTimeCheckTemperature = time + defaultCheckTemperatureInterval;
  }
}

// Check if its filtering time
void checkAutoFiltering() {
  if(time < filteringCycleStopTime)
    autoFiltering = true;
  else
    autoFiltering = false;
}


