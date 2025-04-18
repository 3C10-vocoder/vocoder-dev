#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pin definitions
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_MOSI   9
#define OLED_CLK   10
#define OLED_DC    11
#define OLED_CS    12
#define OLED_RESET 13
#define VRx A0 // x pos of joystick
#define VRy A1 // y pos of joystick
#define SW A2 // click of joystick

//freq display pins
#define BASE_FREQ_PIN A3   // Analog pin for base frequency
#define STEP_FREQ_PIN A4   // Analog pin for step frequency
#define OSCILLOSCOPE_PIN A5

// define drum output pins
#define DRUM_NONE_PIN 4    // No drum selected
#define DRUM_ONE_PIN 5     // First drum (Money beat)
#define DRUM_TWO_PIN 6     // Second drum (Hip-Hop)
#define DRUM_THREE_PIN 7   // Third drum (Funk)

// Constants
const int DEAD_ZONE_UPPER = 1000;
const int DEAD_ZONE_LOWER = 100;
const int MAX_SCREENS = 5;
// const int MAX_DRUM_SCREENS = 3;
const int MAX_SONG_SCREENS = 4;
const char* songNames[] = {"Rick Roll", "Sandstorm", "Enter the Dragon", "None"};

// Globals
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
int screen_state = 1;
// int drum_screen_state = 1;
int song_screen_state = 1;
int selected_song = 0; // 0 means no song selected, 1-3 represents the selected song
int previous_button_state = HIGH; //used to detect click of joystick
int itervar = 0; //used in oscilloscope
int previous_y = 0;

const int MAX_DRUM_SCREENS = 4;
int selected_drum = 0;
int drum_screen_state = 1;
const char* drumNames[] = {"Money beat", "Hip-Hop", "Funk"};

//
double fake_freq = 500;
double base_freq = fake_freq;
double step_freq = fake_freq;


// Function declarations
void drawScreen();
void chooseScreen();
void drawMainMenu(int sensorValue);
void drawSynthScreen();
void drawOscilloscope();
void drawDrumScreen();
void chooseDrumScreen();
void drawSongScreen();
void chooseSongScreen();
void drawPercentageBar(float percentage);

void setup() {
    Serial.begin(9600);
    pinMode(VRx, INPUT);
    pinMode(VRy, INPUT);
    pinMode(SW, INPUT_PULLUP);

    pinMode(BASE_FREQ_PIN, INPUT);  
    pinMode(STEP_FREQ_PIN, INPUT);  

    pinMode(DRUM_NONE_PIN, OUTPUT);
    pinMode(DRUM_ONE_PIN, OUTPUT);
    pinMode(DRUM_TWO_PIN, OUTPUT);
    pinMode(DRUM_THREE_PIN, OUTPUT);
    
    // Default state - "None" selected
    digitalWrite(DRUM_NONE_PIN, HIGH);
    digitalWrite(DRUM_ONE_PIN, LOW);
    digitalWrite(DRUM_TWO_PIN, LOW);
    digitalWrite(DRUM_THREE_PIN, LOW);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

void loop() {
    chooseScreen();
    drawScreen();
}

void drawPercentageBar(float percentage){
  display.print("  [");
  int barLength = percentage / 20; // 20 characters total
  for (int j = 0; j < 5; j++) {
    if (j < barLength) {
      display.print("=");
    } else {
      display.print(" ");
    }
  }
  display.print("]\n");
}

void drawMainMenu(int sensorValue) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Main Menu:");
  // display options
  display.println("1. Synthesizer opts.");
  display.println("2. Oscilloscope.");
  display.println("3. Songs.");
  display.println("4. Drums.");
  display.display();
}

void drawSynthScreen() {

  int baseFreqRaw = analogRead(BASE_FREQ_PIN);
  int stepFreqRaw = analogRead(STEP_FREQ_PIN);
  
  // Calculate percentages (0-100) for display
  float baseFreqPercent = (baseFreqRaw * 100.0) / 1023.0;
  float stepFreqPercent = (stepFreqRaw * 100.0) / 1023.0;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Synth Menu:\n");

  display.print("f:");
  display.print(int(baseFreqPercent)); 
  display.print("%");
  drawPercentageBar(baseFreqPercent);

  
  display.print("g:");
  display.print(int(stepFreqPercent)); 
  display.print("%");
  drawPercentageBar(stepFreqPercent);
  
  display.print("A:");
  display.print(fake_freq);
  display.print("ms");
  drawPercentageBar(20);

  display.print("D:");
  display.print(fake_freq);
  display.print("ms");
  drawPercentageBar(20);

  display.print("S:");
  display.print(fake_freq);
  display.print("% ");
  drawPercentageBar(20);

  display.print("R:");
  display.print(fake_freq);
  display.print("ms");
  drawPercentageBar(20);

  display.display();  // Update the display
  delay(200);
}

void drawOscilloscope() {
  // Read analog value
  int val = analogRead(OSCILLOSCOPE_PIN);
  
  // Scale the value to fit in display height
  // Subtract from SCREEN_HEIGHT to invert (higher values go up)
  int scaledData = map(val, 0, 1023, SCREEN_HEIGHT - 2, 2);
  
  // Clear previous vertical line
  display.drawLine(itervar, 0, itervar, SCREEN_HEIGHT, SSD1306_BLACK);
  
  // Draw a connection line from the previous point to the current point
  display.drawLine(itervar - 1, previous_y, itervar, scaledData, SSD1306_WHITE);
  
  // Draw some grid features
  // Horizontal center line
  if (itervar % 8 == 0) {
    display.drawPixel(itervar, SCREEN_HEIGHT/2, SSD1306_WHITE);
  }
  
  // Show current value in corner
  if (itervar == 1) {
    display.fillRect(0, 0, 30, 8, SSD1306_BLACK);
    display.setCursor(0, 0);
    display.print(val);
  }
  
  // Update display
  display.display();
  
  // Reset to beginning when we reach the edge
  if (itervar >= SCREEN_WIDTH) {
    itervar = 0;
    display.clearDisplay();
  }

  // Store current position for next iteration
  previous_y = scaledData;
  itervar++;
}

void drawDrumScreen(){
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Drum Properties ^ \n");
  chooseDrumScreen();

  switch(drum_screen_state){
    case 1:
      display.print("Kick");
      display.print("10");
      display.print("Hz");
      break;
    case 2:
      display.print("Snare");
      display.print("10");
      display.print("Hz");
      break;
    case 3:
      display.print("Tom");
      display.print("10");
      display.print("Hz");
      break;
  }

  display.display();
}

void chooseDrumScreen(){
  int Yval = analogRead(VRy);

  if(drum_screen_state > MAX_DRUM_SCREENS){ //loop around to main menu
    drum_screen_state = 1; //reset to main menu
    delay(100);
    display.clearDisplay();
  }else if(drum_screen_state < 1){
    drum_screen_state = MAX_DRUM_SCREENS;
    delay(100);
    display.clearDisplay();
  }else{
    if(Yval > DEAD_ZONE_UPPER){ //otherwise check deadzone
      drum_screen_state++; // go right
      delay(100);
      display.clearDisplay();
    }else if(Yval < DEAD_ZONE_LOWER){
      drum_screen_state--; //go left
      delay(100);
      display.clearDisplay();
    }

    Serial.println(drum_screen_state);
  }
}

void drawSongScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print("Songs Menu: ^ v");
  
  // Call the function to handle joystick input
  chooseSongScreen();
  
  // Calculate positions for drawing songs
  int yPos = 12; // Starting y position after the header
  int lineHeight = 12; // Height of each song line
  
  // Draw all song options including "None"
  for(int i = 0; i < MAX_SONG_SCREENS; i++) {
    display.setCursor(10, yPos + (i * lineHeight));
    
    // Add indicator for currently selected song
    if((i + 1 == selected_song) || (i == 3 && selected_song == 0)) {
      display.print("> "); // Add arrow to indicate active selection
    } else {
      display.print("  "); // No indicator
    }
    
    display.print(i < 3 ? songNames[i] : "None");
    
    // Draw box around the currently highlighted (not necessarily selected) song
    if(i + 1 == song_screen_state) {
      display.drawRect(2, yPos + (i * lineHeight) - 2, 124, lineHeight, SSD1306_WHITE);
    }
  }
  
  // Display additional info about the highlighted song if it's not "None"
  if(song_screen_state < 4) {
    display.setCursor(0, yPos + (MAX_SONG_SCREENS * lineHeight) + 4);
    // <TODO> add an actual BPM
    display.print("BPM: ");
    display.print(80 + (song_screen_state * 20));
  }
  
  // show the currently active song at the bottom
  display.setCursor(0, SCREEN_HEIGHT - 8);
  if(selected_song > 0) {
    display.print("Playing: ");
    display.print(songNames[selected_song - 1]);
  } else {
    display.print("No song playing");
  }
  
  display.display();
}

void chooseSongScreen() {
  int Yval = analogRead(VRy);
  int button_state = digitalRead(SW);

  if(song_screen_state > MAX_SONG_SCREENS) { // loop around
    song_screen_state = 1;
    delay(100);
    display.clearDisplay();
  } else if(song_screen_state < 1) {
    song_screen_state = MAX_SONG_SCREENS;
    delay(100);
    display.clearDisplay();
  } else {
    if(Yval > DEAD_ZONE_UPPER) { // move down
      song_screen_state++;
      delay(100);
      display.clearDisplay();
    } else if(Yval < DEAD_ZONE_LOWER) { // move up
      song_screen_state--;
      delay(100);
      display.clearDisplay();
    }
  }

  if (button_state == LOW && previous_button_state == HIGH) {
    // Button was just pressed
    if(song_screen_state == 4) { // "None" option selected
      selected_song = 0;
      Serial.println("No song selected");
    } else {
      selected_song = song_screen_state; // Select the current song
      Serial.print("Selected song: ");
      Serial.println(songNames[selected_song - 1]);
    }
    
    delay(200); // Debounce
  }
  
  previous_button_state = button_state; // Update the button state for next iteration
}

void updateDrumOutputPins() {
  // First turn off all pins
  digitalWrite(DRUM_NONE_PIN, LOW);
  digitalWrite(DRUM_ONE_PIN, LOW);
  digitalWrite(DRUM_TWO_PIN, LOW);
  digitalWrite(DRUM_THREE_PIN, LOW);
  
  // Then set the appropriate pin HIGH based on selection
  switch(selected_drum) {
    case 0:  // None selected
      digitalWrite(DRUM_NONE_PIN, HIGH);
      break;
    case 1:  // Money beat
      digitalWrite(DRUM_ONE_PIN, HIGH);
      break;
    case 2:  // Hip-Hop
      digitalWrite(DRUM_TWO_PIN, HIGH);
      break;
    case 3:  // Funk
      digitalWrite(DRUM_THREE_PIN, HIGH);
      break;
  }
}


void drawDrumSelectionScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print("Drums Menu: ^ v");
  
  // Call the function to handle joystick input
  chooseDrumSelectionScreen();
  
  // Calculate positions for drawing songs
  int yPos = 12; // Starting y position after the header
  int lineHeight = 12; // Height of each song line
  
  // Draw all song options including "None"
  for(int i = 0; i < MAX_DRUM_SCREENS; i++) {
    display.setCursor(10, yPos + (i * lineHeight));
    
    // Add indicator for currently selected song
    if((i + 1 == selected_drum) || (i == 3 && selected_drum == 0)) {
      display.print("> "); // Add arrow to indicate active selection
    } else {
      display.print("  "); // No indicator
    }
    
    display.print(i < 3 ? drumNames[i] : "None");
    
    // Draw box around the currently highlighted (not necessarily selected) song
    if(i + 1 == drum_screen_state) {
      display.drawRect(2, yPos + (i * lineHeight) - 2, 124, lineHeight, SSD1306_WHITE);
    }
  }
  
  // Display additional info about the highlighted song if it's not "None"
  if(drum_screen_state < 4) {
    display.setCursor(0, yPos + (MAX_DRUM_SCREENS * lineHeight) + 4);
    // <TODO> add an actual BPM
    display.print("BPM: ");
    display.print(80 + (drum_screen_state * 20));
  }
  
  // show the currently active song at the bottom
  display.setCursor(0, SCREEN_HEIGHT - 8);
  if(selected_drum > 0) {
    display.print("Playing: ");
    display.print(drumNames[selected_drum - 1]);
  } else {
    display.print("No drum selected");
  }
  
  display.display();
}


void chooseDrumSelectionScreen() {
  int Yval = analogRead(VRy);
  int button_state = digitalRead(SW);

  if(drum_screen_state > MAX_DRUM_SCREENS) { // loop around
    drum_screen_state = 1;
    delay(100);
    display.clearDisplay();
  } else if(drum_screen_state < 1) {
    drum_screen_state = MAX_DRUM_SCREENS;
    delay(100);
    display.clearDisplay();
  } else {
    if(Yval > DEAD_ZONE_UPPER) { // move down
      drum_screen_state++;
      delay(100);
      display.clearDisplay();
    } else if(Yval < DEAD_ZONE_LOWER) { // move up
      drum_screen_state--;
      delay(100);
      display.clearDisplay();
    }
  }

  if (button_state == LOW && previous_button_state == HIGH) {
    // Button was just pressed
    if(drum_screen_state == 4) { // "None" option selected
      selected_drum = 0;
      Serial.println("No song selected");
    } else {
      selected_drum = drum_screen_state; // Select the current song
      Serial.print("Selected song: ");
      Serial.println(songNames[selected_drum - 1]);
    }
    delay(200); // Debounce
    updateDrumOutputPins();
  }

  //write selected drum
  
  previous_button_state = button_state; // Update the button state for next iteration
}




void chooseScreen(){
  int Xval = analogRead(VRx);

  if(screen_state > MAX_SCREENS){ //loop around to main menu
    screen_state = 1; //reset to main menu
    delay(100);
    display.clearDisplay();
  }else if(screen_state < 1){
    screen_state = MAX_SCREENS;
    delay(100);
    display.clearDisplay();
  }else{
    if(Xval > DEAD_ZONE_UPPER){ //otherwise check deadzone
      screen_state++; // go right
      delay(100);
      display.clearDisplay();
    }else if(Xval < DEAD_ZONE_LOWER){
      screen_state--; //go left
      delay(100);
      display.clearDisplay();
    }
  }
}

void drawScreen(){
  switch (screen_state){
    case 1:
      drawMainMenu(1);
      break;
    case 2:
      drawSynthScreen();
      break;
    case 3:
      drawOscilloscope();
      break;
    case 4:
      drawSongScreen();
      break;
    case 5:
      drawDrumSelectionScreen();
      break;
    
    default:
      display.clearDisplay();
      display.display();
      break;
  }
}