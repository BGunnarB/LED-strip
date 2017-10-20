/**
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * LED STRIP sketch for Mysensors
 *******************************
 *
 * REVISION HISTORY
 * 1.0 
 *   Based on the example sketch in mysensors
 * 1.1
 *   fadespeed parameter (send as V_VAR1 message)
 *   HomeAssistant compatible (send status to ack)
 * 1.2
 *   OTA support
 * 1.3
 *   Power-on self test
 * 1.4
 *   Bug fix
 * 1.5
 *   Other default values
 * 1.6
 *   Repeater feature
 * 1.7
 *   Multitasking. Alarm, Relax and normal modes.
 * 1.8
 *   GB Removed Movement and repeater feature. Adapt to rgbw. Adapt to incoming messages from OpenHAB being HEX
 * 2.0  
 *   Different ALARM mode. Changed RELAX colours. Changed SELFTEST.
 * 3.0  
 *   Added possibility to change RELAX matrix via message V_VAR3 9 hex characters. First character 0-7
 *   marks the number of the row to change.
 */

#define MY_NODE_ID 24

#define MY_RADIO_NRF24
// change the pins to free up the pwm pin for led control
#define MY_RF24_CE_PIN 4
#define MY_DEBUG

#include <MySensors.h>

// Arduino pin attached to driver pins
#define REDPIN 3 
#define WHITEPIN 9  
#define GREENPIN 5
#define BLUEPIN 6
#define CHILD_ID_LIGHT 0
#define NUM_CHANNELS 4  //Four channel LED-strip (r, g, b, w)

#define SN "LED Strip"
#define SV "3.0"

MyMessage lightMsg(CHILD_ID_LIGHT, V_LIGHT);
MyMessage rgbMsg(CHILD_ID_LIGHT, V_RGBW);
MyMessage dimmerMsg(CHILD_ID_LIGHT, V_DIMMER);
MyMessage fadeMsg(CHILD_ID_LIGHT, V_VAR2);


int current_r = 255;
int current_g = 255;
int current_b = 255;
int current_w = 255;
int target_r = 255;
int target_g = 255;
int target_b = 255;
int target_w = 255;
int save_r;
int save_g;
int save_b;
int save_w;
byte target_values[4] = {100, 100, 100, 100};


float delta_r = 0.0;
float delta_g = 0.0;
float delta_b = 0.0;
float delta_w = 0.0;

char rgbstring[] = "ffffffff";
char rlxstring[] = "0aaaaaaaa"; // pattern for message to fill program_param_RELAX
int  pos = 0; // position (row) to store new relax pattern

int on_off_status = 0;
int dimmerlevel = 100;
int fadespeed = 200;
unsigned long last_update = 0;
int tick_length = 10;
int fade_step = 0;

int program_timer;
int program_cycle;
boolean rlX; // flag to mark that this is a relax matrix message

#define LIGHT_NORMAL 0
#define LIGHT_FADING 1

#define PROGRAM_NORMAL 0
#define PROGRAM_ALARM 1
#define PROGRAM_RELAX 2

int light_mode = LIGHT_NORMAL;
int program_mode = PROGRAM_NORMAL;

#define RELAX_SPEED 16  //Maximum is 16 to keep fadespeed * RELAX_SPEED <= 32000
#define MAX_CYCLES_RELAX 8
int program_param_RELAX[MAX_CYCLES_RELAX][4] = {
  {255, 0, 0, 0},   
  {1, 255, 1, 1}, 
  {2, 2, 255, 2}, 
  {3, 3, 3, 255},   
  {4, 255, 4, 4}, 
  {5, 5, 255, 5},
  {6, 6, 6, 255},
  {7, 7, 7, 7}    
};

#define ALARM_SPEED 800
#define MAX_CYCLES_ALARM 6
const int program_param_ALARM[MAX_CYCLES_ALARM][4] = {
  {0, 0, 0, 0},    //00 00 00 00
  {128, 0, 0, 128}, //80 00 00 80
  {128, 0, 0, 255}, //80 00 00 ff
  {128, 0, 0, 255}, //80 00 00 ff
  {128, 0, 0, 128}, //80 00 00 80
  {0, 0, 0, 0}     //00 00 00 00
};

void setup()
{
  TCCR0B = TCCR0B & B11111000 | B00000010;    // set timer 0 divisor(G and B) to 8 for PWM frequency of  7812.50 Hz
  // to avoid interference with PWM frequency of timer for R and W channels

  // Output pins
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  pinMode(WHITEPIN, OUTPUT);
  
}

void presentation()
{
  // Send the Sketch Version Information to the Gateway
  sendSketchInfo(SN, SV);
  present(CHILD_ID_LIGHT, S_RGBW_LIGHT);

}

void selftest() {
  on_off_status = 1;
  current_r = 255;
  current_g = 0;
  current_b = 0;
  set_hw_status();
  wait(750);
  current_r = 0;
  current_g = 255;
  set_hw_status();
  wait(750);
  current_g = 0;
  current_b = 255;
  set_hw_status();
  wait(750);
  current_r = 255;
  current_g = 255;
  set_hw_status();
  wait(750);
  current_r = 0;
  current_g = 0;
  current_b = 0;
  current_w = 255;
  set_hw_status();
  wait(750);
  current_w = 0;
  on_off_status = 0;
}


void loop()
{
  static bool first_message_sent = false;
  if ( first_message_sent == false ) {
    selftest();
    set_hw_status();
    send_status(1, 1, 1);
    first_message_sent = true;
  }

  unsigned long now = millis();
  
  if (now - last_update > tick_length) {
    last_update = now;

    if (light_mode == LIGHT_FADING) {
      calc_fade();
    }

    if (program_mode > PROGRAM_NORMAL) {
      handle_program();
    }
    
  }
  set_hw_status();
  
}

void receive(const MyMessage &message)
{
  int val;
  
  if (message.type == V_RGBW) {
    Serial.print( "V_RGBW: " );
    Serial.println(message.data);
    const char * rgbvalues = message.getString();
    inputToRGBW(rgbvalues);
    // Save old value
    strcpy(rgbstring, message.data);

    //Copy target values to r, g, b, w
    int r = target_values[0];
    int g = target_values[1];
    int b = target_values[2];
    int w = target_values[3];
    
    init_fade(fadespeed, r, g, b, w); 
    send_status(0, 0, 1);

  } else if (message.type == V_LIGHT || message.type == V_STATUS) {
    Serial.print( "V_STATUS: " );
    Serial.println(message.data);
    val = atoi(message.data);
    if (val == 0 or val == 1) {
      on_off_status = val;
      send_status(1, 0, 0);
    }
    
  } else if (message.type == V_DIMMER || message.type == V_PERCENTAGE) {
    Serial.print( "V_PERCENTAGE: " );
    Serial.println(message.data);
    val = atoi(message.data);
    if (val >= 0 and val <=100) {
      dimmerlevel = val;
      send_status(0, 1, 0);
    }
    
  } else if (message.type == V_VAR1 ) {
    Serial.print( "V_VAR1: " );
    Serial.println(message.data);
    val = atoi(message.data);
    if (val >= 0 and val <= 2000) {
      fadespeed = val;
    } else fadespeed = 20;

  } else if (message.type == V_VAR2 ) {
    Serial.print( "V_VAR2: " );
    Serial.println(message.data);
    val = atoi(message.data);
    if (val == PROGRAM_NORMAL) {
      stop_program();
    } else if (val == PROGRAM_ALARM || val == PROGRAM_RELAX) {
      init_program(val);
    }
    
  } else if (message.type == V_VAR3 ) {
    Serial.print( "V_VAR3: " );
    Serial.println(message.data);
    rlX = true; // set flag to mark relax matrix values
    const char * rgbvalues = message.getString();
    inputToRGBW(rgbvalues);
    
    for (int i = 0; i < MAX_CYCLES_RELAX; i++) {
      for (int j = 0; j < NUM_CHANNELS; j++) {
        Serial.print(String(program_param_RELAX[i][j]) + (", "));
      }
    }
    Serial.println();
    rlX = false; // reset flag
  }

    else {
    Serial.println( "Invalid command received..." );
    return;
  }

}

void set_rgb(int r, int g, int b, int w) {
  analogWrite(REDPIN, r);
  analogWrite(GREENPIN, g);
  analogWrite(BLUEPIN, b);
  analogWrite(WHITEPIN, w);
}

void init_program(int program) {
  program_mode = program;
  program_cycle = 0;
  save_rgb();
  
  if (program == PROGRAM_ALARM) {
    program_timer = MAX_CYCLES_ALARM;
    init_fade(fadespeed,
              program_param_ALARM[program_cycle][0],
              program_param_ALARM[program_cycle][1],
              program_param_ALARM[program_cycle][2],
              program_param_ALARM[program_cycle][3]);
  } 
  else if (program == PROGRAM_RELAX) {
    init_fade(fadespeed,
              program_param_RELAX[program_cycle][0],
              program_param_RELAX[program_cycle][1],
              program_param_RELAX[program_cycle][2],
              program_param_RELAX[program_cycle][3]);
  }
}

void handle_program() {
  if (program_mode == PROGRAM_ALARM) {
    if (program_timer >= 0) {
      if (light_mode == LIGHT_NORMAL) {
        program_cycle = (program_cycle+1) % MAX_CYCLES_ALARM;
        Serial.print("Next cycle step ");
        Serial.println(program_cycle);
        init_fade(ALARM_SPEED,
                program_param_ALARM[program_cycle][0],
                program_param_ALARM[program_cycle][1],
                program_param_ALARM[program_cycle][2],
                program_param_ALARM[program_cycle][3]); // fixed speed of alarm sequence
        Serial.println(program_timer);  // only do one set of cycles
        program_timer--;
      }
    }
    else stop_program(); // then reset alarm status  
  }
    else if (program_mode == PROGRAM_RELAX) {
    if (light_mode == LIGHT_NORMAL) {
      program_cycle = (program_cycle+1) % MAX_CYCLES_RELAX;
      Serial.print("Next cycle step ");
      Serial.println(program_cycle);
      init_fade(fadespeed * RELAX_SPEED,
                program_param_RELAX[program_cycle][0],
                program_param_RELAX[program_cycle][1],
                program_param_RELAX[program_cycle][2],
                program_param_RELAX[program_cycle][3]);
    
    } 
  }
}

void stop_program() {
  restore_rgb();
  light_mode = LIGHT_NORMAL;
  program_mode = PROGRAM_NORMAL;
}

void save_rgb() {
  save_r = current_r;
  save_g = current_g;
  save_b = current_b;
  save_w = current_w;
}

void restore_rgb() {
  current_r = save_r;
  current_g = save_g;
  current_b = save_b;
  current_w = save_w;
}
void init_fade(int t, int r, int g, int b, int w) {
  Serial.println( "Init fade " );
  light_mode = LIGHT_FADING;
  target_r = r;
  target_g = g;
  target_b = b;
  target_w = w;
  fade_step = t;
  if (fade_step <= 0) {fade_step = 1;}  // just checking that we do not get overflow in division!
  delta_r = (target_r - current_r) / float(fade_step);
  delta_g = (target_g - current_g) / float(fade_step);
  delta_b = (target_b - current_b) / float(fade_step);
  delta_w = (target_w - current_w) / float(fade_step);
}

void calc_fade() {
  if (fade_step > 0) {
    fade_step--;
    current_r = target_r - delta_r * fade_step;
    current_g = target_g - delta_g * fade_step;
    current_b = target_b - delta_b * fade_step;
    current_w = target_w - delta_w * fade_step;
  } else {
    Serial.println( "Fade ready" );
    light_mode = LIGHT_NORMAL;
  } 
}

void set_hw_status() {
  int r = on_off_status * (int)(current_r * dimmerlevel/100.0);
  int g = on_off_status * (int)(current_g * dimmerlevel/100.0);
  int b = on_off_status * (int)(current_b * dimmerlevel/100.0);
  int w = on_off_status * (int)(current_w * dimmerlevel/100.0);

  set_rgb(r, g, b, w);
  
}


void send_status(int send_on_off_status, int send_dimmerlevel, int send_rgbstring) {
  if (send_rgbstring) send(rgbMsg.set(rgbstring));
  if (send_rgbstring) send(fadeMsg.set(fadespeed));
  if (send_on_off_status) send(lightMsg.set(on_off_status));
  if (send_dimmerlevel) send(dimmerMsg.set(dimmerlevel));
}

void inputToRGBW(const char * input) {
  Serial.print("Got color value of length: "); 
  Serial.println(strlen(input));
  
  
  if (strlen(input) == 8) {
    Serial.println("new rgbw value");
    target_values[0] = fromhex (& input [0]);
    target_values[1] = fromhex (& input [2]);
    target_values[2] = fromhex (& input [4]);
    target_values[3] = fromhex (& input [6]);
  } 
  else if (strlen(input) == 9 && rlX) {
    Serial.println("new relax array values");
    pos = input [0] - '0';
    if (pos >= 0 && pos < MAX_CYCLES_RELAX) {
      program_param_RELAX[pos][0] = fromhex (& input [1]);
      program_param_RELAX[pos][1] = fromhex (& input [3]);
      program_param_RELAX[pos][2] = fromhex (& input [5]);
      program_param_RELAX[pos][3] = fromhex (& input [7]);
    }
  } 
    
  else {
    Serial.println("Wrong length of input");
  }  



  Serial.print("New color values: ");
  Serial.println(input);
  
  for (int i = 0; i < NUM_CHANNELS; i++) {
    Serial.print(target_values[i]);
    Serial.print(", ");
  }
 
}

// converts hex char to byte
byte fromhex (const char * str)
{
  char c = str [0] - '0';
  if (c > 9) c -= 7;
  if (c > 41) c -=32;
  int result = c;
  c = str [1] - '0';
  if (c > 9) c -= 7;
  if (c > 41) c -=32;
  return (result << 4) | c;
}
