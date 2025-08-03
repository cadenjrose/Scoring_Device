/*===================================================================*\   
|                              META DATA                              |
\*===================================================================*/

// Filename------------+ scorer.cpp
// Date Created--------+ 8/2/2025
// Date Last Modified--+ 8/2/2025
// Description---------+ Controls four 7-segment displays used to display scores
// --------------------- for 2 players (2 button inputs) (7 segs are comm. Anode)
// Features------------+ Score incrementing, winning conditions (up to 21 win by
// --------------------- 2, score flashing when winning conditions are met, game
// --------------------- reset with 3 sec button hold

/*===================================================================*\   
|                             BOARD LEVEL                             |
\*===================================================================*/

// Board---------------+ Arduino Mega or Mega 2560
// Processor-----------+ ATmega2560 (Mega 2560)
// Programmer----------+ AVRISP mkll
// Output Pins---------+ 2-8, 11, 14-20, 22-35
// Input Pins----------+ 9-10                                         
                                                                     /*
     7 seg display          7 Seg Common Anode Output
                               A  B  C  D  E  F  G   hex
          A                 0: 0, 0, 0, 0, 0, 0, 1   0x2
      +--------+            1: 1, 0, 0, 1, 1, 1, 1   0x9E
      |        |            2: 0, 0, 1, 0, 0, 1, 0   0x24
    F |   G    | B          3: 0, 0, 0, 0, 1, 1, 0   0xC
      +--------+            4: 1, 0, 0, 1, 1, 0, 0   0x98
      |        |            5: 0, 1, 0, 0, 1, 0, 0   0x48
    E |        | C          6: 0, 1, 0, 0, 0, 0, 0   0x40
      +--------+            7: 0, 0, 0, 1, 1, 1, 1   0x1D
          D                 8: 0, 0, 0, 0, 0, 0, 0   0x0
                            9: 0, 0, 0, 0, 1, 0, 0   0x8

/*===================================================================*\   
|                         PREPROCESSOR MACROS                         |
\*===================================================================*/

// Button Pins
#define P1_BUTTON 10         // Player 1 Button Input Pin
#define P2_BUTTON 9          // Player 2 Button Input Pin

// Reset
#define RESET 11             // Pin tied to RESET

// Game Configuration
#define BUTTON_HOLD_MS 3000     // Button hold threshold to reset game
#define SCORE_BLINK_MS 500      // Length of time between winning score blinks
#define BUTTON_PRESS_LENGTH 200 // Approx. length of time of a button press
#define UP_TO_SCORE 21          // Score to play up to

// 7 Segment Configuration
#define SEVEN_SEGMENTS 7     // # of segments used
#define NUM_DIGITS 10        // # of digits per display
#define COMMON_ANODE         // Define Common Anode as 7 Segment Type

// Common Type
#ifdef COMMON_ANODE
#define ON LOW
#define OFF HIGH
#else
#define ON HIGH
#define OFF LOW
#endif

/*===================================================================*\   
|                           TYPE DEFINITIONS                          |
\*===================================================================*/

/*
 * Player type keeps track of its pin assignments, score digit values,
 * button hold start time, and button states (current & previous)
 */
typedef struct{
  uint8_t d1_pins[7];     // Pin numbers for first digit display
  uint8_t d2_pins[7];     // Pin numbers for second digit display
  uint8_t d1_num;         // Tens place score value
  uint8_t d2_num;         // Ones Place score value
  unsigned long start;    // Start time for button hold period
  bool button_state;      // 1 = button pressed
  bool prev_button_state; // 0 = last state was off
} Player;

/*===================================================================*\   
|                           GLOBAL VARIABLES                          |
\*===================================================================*/

Player p1; // Player type for player 1
Player p2; // Player type for player 2
bool winner_found; // Winner found flag
bool p1_is_winner; // TRUE = Player 1 has won, FALSE = Player 2 has won

/*
 * Segment level values to display digits 
 * ( COMMON ANODE )
*/
byte displayLEDs[NUM_DIGITS][SEVEN_SEGMENTS] = 
{
    {ON, ON, ON, ON, ON, ON, OFF},    // 0
    {OFF, ON, ON, ON, ON, ON, OFF},   // 1
    {ON, ON, OFF, ON, ON, OFF, ON},   // 2
    {ON, ON, ON, ON, OFF, OFF, ON},   // 3
    {OFF, ON, ON, OFF, OFF, ON, ON},  // 4
    {ON, OFF, ON, ON, OFF, ON, ON},   // 5
    {ON, OFF, ON, ON, ON, ON, ON},    // 6
    {ON, ON, ON, OFF, OFF, OFF, OFF}, // 7
    {ON, ON, ON, ON, ON, ON, ON},     // 8
    {ON, ON, ON, ON, OFF, ON, ON}     // 9
};

/*===================================================================*\   
                             FUNCTIONS                                |
\*===================================================================*/

/*
 * @brief Displays a tens place value
 * @param p Player to update
 * @param num Value to update to
 * In Range Values : 0 -> 9
 * Out of range : displays blank segment
*/
void displayFirstDigit(const Player& p, int num){
  for( int i = 0; i < SEVEN_SEGMENTS; i++){
    if(num < 0 || num >= NUM_DIGITS) {
        digitalWrite(p.d1_pins[i], OFF);  // all segments off
    } else {
        digitalWrite(p.d1_pins[i], displayLEDs[num][i]);
    }
  }
}

/*
 * @brief Displays a ones place value
 * @param p   -> Player to update
 * @param num -> Value to update to (blank if out of range)
 * In Range Values : 0 -> 9
 * Out of range : displays blank segment
*/
void displaySecondDigit(const Player& p, int num){
  for( int i = 0; i < SEVEN_SEGMENTS; i++){
    if(num < 0 || num >= NUM_DIGITS) {
        digitalWrite(p.d2_pins[i], OFF);  // all segments off
    } else {
        digitalWrite(p.d2_pins[i], displayLEDs[num][i]);
    }
  }
}

/*
 * @brief Blinks the score of the provided player
 * @param p -> The winning player
*/
void blinkWinner(const Player& p) {
  displayFirstDigit(p, -1);  // displays blank
  displaySecondDigit(p, -1); // displays blank
  delay(SCORE_BLINK_MS);
  displayFirstDigit(p, p.d1_num);
  displaySecondDigit(p, p.d2_num);
  delay(SCORE_BLINK_MS);
}

/*
 * @brief Conducts a full board reset 
*/
void reset_game() {
  pinMode(RESET, OUTPUT); // signals reset pin
}

/*
 * @brief Handles button events for p (Pressed, Held, Released)
 * @param p Player to handle button of
*/
void handle_button(Player& p) {
  // DETERMINE BUTTON STATE
  p.button_state = digitalRead((&p == &p1) ? P1_BUTTON : P2_BUTTON);

  // ON BUTTON PRESS
  if(p.button_state && !p.prev_button_state) {
    p.start = millis();
    delay(BUTTON_PRESS_LENGTH);
  }
  // ON BUTTON HOLD
  else if(p.button_state && p.prev_button_state) {
    if(millis() - p.start >= BUTTON_HOLD_MS) { // hold has exceeded time limit
      reset_game();
    }
  }
  // ON BUTTON RELEASE
  else if(!p.button_state && p.prev_button_state) {
    if(!winner_found){
      // INCREMENT SCORE
      p.d2_num++; 
      if(p.d2_num >= NUM_DIGITS){
        p.d1_num++;
        p.d2_num = 0;              
      }
    }
  }
  
  p.prev_button_state = p.button_state;
}


/*===================================================================*\   
|                                SETUP()                              |
\*===================================================================*/

void setup() {
  // INITIALIZE GLOBALS
  winner_found = false;
  p1_is_winner = false;

  // =========== Player 1 ============ //
  p1 = { 
    .d1_pins = {2,3,4,5,6,7,8},
    .d2_pins = {14,15,16,17,18,19,20},
    .d1_num = 0,
    .d2_num = 0,
    .start = 0,
    .button_state = LOW,
    .prev_button_state = LOW
  };

  // =========== Player 2 ============ //
  p2 = { 
    .d1_pins = {22,24,26,28,30,32,34},
    .d2_pins = {23,25,27,29,31,33,35},
    .d1_num = 0,
    .d2_num = 0,
    .start = 0,
    .button_state = LOW,
    .prev_button_state = LOW
  };

  // SET OUTPUT PINS
  for(int i = 0; i < SEVEN_SEGMENTS; i++){
    pinMode(p1.d1_pins[i], OUTPUT);
    pinMode(p1.d2_pins[i], OUTPUT);
    pinMode(p2.d1_pins[i], OUTPUT);
    pinMode(p2.d2_pins[i], OUTPUT);
  }

  // SET INPUT PINS
  pinMode(P1_BUTTON, INPUT);
  pinMode(P2_BUTTON, INPUT);
}

/*===================================================================*\   
|                                 LOOP()                              |
\*===================================================================*/

void loop() {
  // DISPLAY SCORES
  displayFirstDigit(p1, p1.d1_num);
  displaySecondDigit(p1, p1.d2_num);
  displayFirstDigit(p2, p2.d1_num);
  displaySecondDigit(p2, p2.d2_num);

  // HANDLE BUTTON INPUTS
  handle_button(p1);
  handle_button(p2);
  
  // CHECK FOR WINNING CONDITIONS
  if(!winner_found) {
     // COMBINE DIGIT NUMS INTO SCORE
     uint8_t p1_score = p1.d1_num * NUM_DIGITS + p1.d2_num;
     uint8_t p2_score = p2.d1_num * NUM_DIGITS + p2.d2_num;

     // CHECK WIN BY 2
     if(p1_score >= UP_TO_SCORE && p1_score > (p2_score + 1)) {
       winner_found = true;
       p1_is_winner = true;
     } else if(p2_score >= UP_TO_SCORE && p2_score > (p1_score + 1)) {
       winner_found = true;
     }
   } else if(p1_is_winner){
     // BLINK PLAYER 1's SCORE
     blinkWinner(p1);
   } else {
     // BLINK PLAYER 2's SCORE
     blinkWinner(p2);
   }
}
// EOF
