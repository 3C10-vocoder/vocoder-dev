#define OUTPUT_PIN A5


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

int itervar = 0;
int previous_y = 0; 
int iter2 = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void setup() {
    Serial.begin(200000);
    pinMode(OUTPUT_PIN, INPUT);
    
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
    double val = analogRead(OUTPUT_PIN);
    //double val = 32 + 32*(sin(iter2 * 5 * PI/180));
    Serial.println(val);

    int scaledData = map(val, 0, 1023, 0, SCREEN_HEIGHT);

/*

    display.drawPixel( 0 , 0, 0, SCREEN_HEIGHT, SSD1306_BLACK);
    display.drawPixel(itervar, 0, itervar, SCREEN_HEIGHT, SSD1306_BLACK);
    display.drawPixel(itervar - 1, previous_y, itervar, scaledData, SSD1306_WHITE);
*/
    display.drawLine(0, 0, 0, SCREEN_HEIGHT, SSD1306_BLACK);
    display.drawLine(itervar, 0, itervar, SCREEN_HEIGHT, SSD1306_BLACK);
    display.drawLine( itervar - 1, previous_y, itervar, scaledData, SSD1306_WHITE);
    display.display();
    
    if (itervar >= SCREEN_WIDTH) {
      itervar = 0;
    }

    previous_y = scaledData;
    itervar++;
    iter2++;

    //delay(10);
    
}