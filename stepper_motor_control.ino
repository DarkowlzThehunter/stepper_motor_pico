#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

#define ALARM_NUM_0 0
#define ALARM_IRQ_0 TIMER_IRQ_0

int DELAY_0 = 3000;  // 1/Fs (in microseconds) /100000 = 1sec 

const int pins_MTR[] = { 11, 12, 13, 14, 15 };
const int MD_MODE = 11;
const int AIN1 = 12;
const int AIN2 = 13;
const int BIN1 = 14;
const int BIN2 = 15;

int current_idx = 0;
volatile int step_counter = 0;
volatile int G_steps = 0;
int x = 1;  // Direction variable
volatile int current_step = 0;
volatile int Limit_1 = 999999;
volatile int Limit_2 = -999999;

void cycle(int idx) {
  switch (idx) {
    case 0:
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);      
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, LOW);
      break;
    case 1:
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      break;
    case 2:
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      break;
    case 3:
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      break;
    case 4:
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, LOW);
      break;
    case 5:
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      break;
    case 6:
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      break;
    case 7:
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      break;
  }
}

// Alarm ISR
static void alarm_irq_0(void) {
  current_idx = (current_idx + x + 8) % 8;
  current_step = current_step + x;
  // Clear the alarm irq
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM_0);
  step_counter++;
  cycle(current_idx);  // Call cycle to update motor state
  if (step_counter >= G_steps) {
    irq_set_enabled(ALARM_IRQ_0, false);  // Disable the timer interrupt
  } else {
    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM_0] = timer_hw->timerawl + DELAY_0;
  }
}

void forward(int L_steps) {
  if (L_steps < 0) {
    backward(-L_steps);
    return;
  }
  x = 1;
  step_counter = 0;
  G_steps = L_steps;
  irq_set_enabled(ALARM_IRQ_0, true);                           // Enable the timer interrupt
  timer_hw->alarm[ALARM_NUM_0] = timer_hw->timerawl + DELAY_0;  // Start the timer
  while (irq_is_enabled(ALARM_IRQ_0)) {
    // Check if the reed switch is triggered
    if (digitalRead(4) == HIGH) {
      irq_set_enabled(ALARM_IRQ_0, false);  // Disable the timer interrupt
      Serial.println("Limit or obstacle detected! Stopping movement.");
      Limit_1 = current_step; //Set limit
      break;
      }
    if (Limit_1 <= current_step){irq_set_enabled(ALARM_IRQ_0, false); Serial.println("Limit1"); break;}
    if (Limit_2 >= current_step){irq_set_enabled(ALARM_IRQ_0, false); Serial.println("Limit2"); break;}
  }
}

void backward(int L_steps) {
  if (L_steps < 0) {
    forward(-L_steps);
    return;
  }
  x = -1;
  step_counter = 0;
  G_steps = L_steps;
  irq_set_enabled(ALARM_IRQ_0, true);                           // Enable the timer interrupt
  timer_hw->alarm[ALARM_NUM_0] = timer_hw->timerawl + DELAY_0;  // Start the timer
  while (irq_is_enabled(ALARM_IRQ_0)) {
    // Check if the reed switch is triggered
    if (digitalRead(5) == HIGH) {
      irq_set_enabled(ALARM_IRQ_0, false);  // Disable the timer interrupt
      Serial.println("Limit or obstacle detected! Stopping movement.");
      Limit_2 = current_step;
      break;
      }
    if (Limit_1 <= current_step){irq_set_enabled(ALARM_IRQ_0, false); break;} //+    
    if (Limit_2 >= current_step){irq_set_enabled(ALARM_IRQ_0, false); break;} //-
  }
}

void rehome() {
  forward(-current_step);
}

void setup() {
  for (int i = 0; i < 5; i++) {
    pinMode(pins_MTR[i], OUTPUT);
  }
  digitalWrite(MD_MODE, LOW);

  // Enable the interrupt for the first alarm (ALARM_NUM_0)
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM_0);
  // Associate an interrupt handler with ALARM_IRQ_0
  irq_set_exclusive_handler(ALARM_IRQ_0, alarm_irq_0);
  // Initially disable the first alarm interrupt
  irq_set_enabled(ALARM_IRQ_0, false);
  Serial.begin(115200);
  //For Wifi module
  pinMode(3,OUTPUT);
  digitalWrite(3,HIGH);
  //for reed switch
  pinMode(4, INPUT_PULLDOWN);
  pinMode(5, INPUT_PULLDOWN);
  //
  while (!Serial);
  Serial.print("STARTING\n");
  
  Serial.print("Current Delay= ");
  Serial.println(DELAY_0);
  Serial.println("1:Forward, 2:Backward, 3:ToStartPoint, 4:Change Speed");
}

int com_num = 0;


void loop() {
  // if(digitalRead(5) == HIGH){Serial.println("5 is High");}
  // if(digitalRead(4) == HIGH){Serial.println("4 is High");}

  if (Serial.available() > 0) {
    String com = Serial.readStringUntil('\n');
    com.trim();
    com_num = com.toInt();

    if (com.length() == 0) {
      Serial.println("No input received.");
    } else if (com_num == 0 && com != "0") {
      Serial.println("Invalid input. Please enter a valid integer.");
    } else {
      Serial.println(com_num);
      int steps = 0;

      switch (com_num) {
        case 1:
          Serial.println("How many step(s)?");
          while (!Serial.available());
          steps = Serial.readStringUntil('\n').toInt();
          Serial.print(steps);
          Serial.println(" steps");          
          forward(steps);
          break;

        case 2:
          Serial.println("How many step(s)?");
          while (!Serial.available());
          steps = Serial.readStringUntil('\n').toInt();
          Serial.print(steps);
          Serial.println(" steps");
          backward(steps);
          break;

        case 3:
          Serial.println("Returning to start point.");
          Serial.print(current_step);
          Serial.println(" steps");
          rehome();
          break;
        case 4: //change the delay of the alarm, which will change the speed of the stepper motor.
          Serial.println("Changing Delay");
          while (!Serial.available());
          DELAY_0 = Serial.readStringUntil('\n').toInt();
          Serial.print(DELAY_0);
          Serial.println(" <--- delay");
          break;
        default:
          Serial.println("Please enter a valid integer.");
          break;
      }
    }
  }
}