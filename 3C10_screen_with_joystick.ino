#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected using software SPI (default case):
#define OLED_MOSI   9
#define OLED_CLK   10
#define OLED_DC    11
#define OLED_CS    12
#define OLED_RESET 13

#define VRx A0 // x pos of joystick
#define VRy A1 // y pos of joystick
#define SW A2 // click of joystick

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void setup() {
    Serial.begin(9600);
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);
    pinMode(A2, INPUT);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();

}

const int DEAD_ZONE_UPPER = 1000;
const int DEAD_ZONE_LOWER = 100;
int screen_state = 1;
int MAX_SCREENS = 4;
int drum_screen_state = 1;
int MAX_DRUM_SCREENS = 3;

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
  display.println("Main Menu");
  // Display dynamic parameter, e.g., sensor reading
  display.print("Value: ");
  display.println(sensorValue);
  display.display();  // Update the display
}


double fakefreq = 500;
void drawSynthScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Synth Menu:\n");

  // Display dynamic params
  
  display.print("f:");
  display.print(fakefreq);
  display.print("Hz");
  drawPercentageBar(20);

  display.print("g:");
  display.print(fakefreq);
  display.print("Hz");
  drawPercentageBar(20);
  
  display.print("A:");
  display.print(fakefreq);
  display.print("ms");
  drawPercentageBar(20);

  display.print("D:");
  display.print(fakefreq);
  display.print("ms");
  drawPercentageBar(20);

  display.print("S:");
  display.print(fakefreq);
  display.print("% ");
  drawPercentageBar(20);

  display.print("R:");
  display.print(fakefreq);
  display.print("ms");
  drawPercentageBar(20);

  display.display();  // Update the display
}


int itervar = 0;
int previous_y = 0; 
void drawOscilloscope(){

  double val = 100;
  int scaledData = map(val, 0, 1023, 10, SCREEN_HEIGHT);
  display.drawLine(0, 0, 0, SCREEN_HEIGHT, SSD1306_BLACK);
  display.drawLine(itervar, 0, itervar, SCREEN_HEIGHT, SSD1306_BLACK);
  display.drawLine( itervar - 1, previous_y, itervar, scaledData, SSD1306_WHITE);
  display.display();
  
  if (itervar >= SCREEN_WIDTH) {
    itervar = 0;
  }

  previous_y = scaledData;
  itervar++;
}


void drawDrumScreen(){
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Drum Properties^ \n");
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
      //Serial.println("UPPER");
      delay(100);
      display.clearDisplay();
    }else if(Xval < DEAD_ZONE_LOWER){
      screen_state--; //go left
      //Serial.println("LOWER");
      delay(100);
      display.clearDisplay();
    }

    //Serial.println(screen_state);
  }
  

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
      //Serial.println("UPPER");
      delay(100);
      display.clearDisplay();
    }else if(Yval < DEAD_ZONE_LOWER){
      drum_screen_state--; //go left
      //Serial.println("LOWER");
      delay(100);
      display.clearDisplay();
    }

    Serial.println(drum_screen_state);
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
      drawDrumScreen();
      break;
    default:
      display.clearDisplay();
      display.display();
      break;

  }

}


