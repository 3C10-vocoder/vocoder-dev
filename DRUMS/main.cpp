#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "pico/time.h"
#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"
#include "textRenderer/5x8_font.h" // Include the correct font file

// Pin definitions for Pico
#define I2C_PORT i2c0
#define I2C_SDA 4  // GPIO 4 (Pin 6)
#define I2C_SCL 5  // GPIO 5 (Pin 7)
#define VRx 26     // GPIO 26 (ADC0)
#define VRy 27     // GPIO 27 (ADC1)
#define SW  28     // GPIO 28 (ADC2)

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

// Constants
const int DEAD_ZONE_UPPER = 3000;
const int DEAD_ZONE_LOWER = 1000;
const int MAX_SCREENS = 4;
const int MAX_DRUM_SCREENS = 3;
const int MAX_SONG_SCREENS = 4;
const char* songNames[] = {"Rick Roll", "Sandstorm", "Enter the Dragon", "None"};

// Globals - we'll initialize this in main()
pico_ssd1306::SSD1306* display;

int screen_state = 1;
int drum_screen_state = 1;
int song_screen_state = 1;
int selected_song = 0; // 0 means no song selected, 1-3 represents the selected song
int previous_button_state = 1; // used to detect click of joystick
int itervar = 0; // used in oscilloscope
int previous_y = 0;

// Fake parameter for testing
double fakefreq = 500;

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
void drawPercentageBar(float percentage, int x, int y);

// DIY drawing functions since we can't rely on the documented ones
void drawLine(pico_ssd1306::SSD1306* disp, int x0, int y0, int x1, int y1, pico_ssd1306::WriteMode mode);
void drawRect(pico_ssd1306::SSD1306* disp, int x, int y, int w, int h, pico_ssd1306::WriteMode mode);

int main() {
    stdio_init_all();
    
    // Initialize I2C
    i2c_init(I2C_PORT, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Initialize ADC
    adc_init();
    adc_gpio_init(VRx);  // ADC0
    adc_gpio_init(VRy);  // ADC1
    adc_gpio_init(SW);   // ADC2
    
    // Initialize switch with pull-up
    gpio_set_pulls(SW, true, false);
    
    // Initialize display using the constructor directly
    display = new pico_ssd1306::SSD1306(I2C_PORT, OLED_ADDRESS, pico_ssd1306::Size::W128xH64);
    
    // Clear the display
    display->clear();
    
    // Main loop
    while (1) {
        chooseScreen();
        drawScreen();
        sleep_ms(10);  // Small delay to prevent too fast updates
    }
    
    return 0;
}

// Our own implementation of drawing functions since the documented ones don't exist
void drawLine(pico_ssd1306::SSD1306* disp, int x0, int y0, int x1, int y1, pico_ssd1306::WriteMode mode) {
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    
    while (true) {
        disp->setPixel(x0, y0, mode);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            if (x0 == x1) break;
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            if (y0 == y1) break;
            err += dx;
            y0 += sy;
        }
    }
}

void drawRect(pico_ssd1306::SSD1306* disp, int x, int y, int w, int h, pico_ssd1306::WriteMode mode) {
    // Draw horizontal lines
    drawLine(disp, x, y, x + w, y, mode);
    drawLine(disp, x, y + h, x + w, y + h, mode);
    
    // Draw vertical lines
    drawLine(disp, x, y, x, y + h, mode);
    drawLine(disp, x + w, y, x + w, y + h, mode);
}

void drawPercentageBar(float percentage, int x, int y) {
    char buffer[20];
    
    // Draw progress bar
    sprintf(buffer, "  [");
    pico_ssd1306::drawText(display, font_5x8, buffer, x, y);
    x += 15; // Move position to after the bracket
    
    int barLength = percentage / 20; // 20 characters total
    for (int j = 0; j < 5; j++) {
        if (j < barLength) {
            pico_ssd1306::drawText(display, font_5x8, "=", x + (j * 6), y);
        } else {
            pico_ssd1306::drawText(display, font_5x8, " ", x + (j * 6), y);
        }
    }
    
    pico_ssd1306::drawText(display, font_5x8, "]", x + 30, y);
}

void drawMainMenu(int sensorValue) {
    display->clear();
    
    // Display options
    pico_ssd1306::drawText(display, font_5x8, "Main Menu:", 0, 0);
    pico_ssd1306::drawText(display, font_5x8, "1. Synthesizer opts.", 0, 12);
    pico_ssd1306::drawText(display, font_5x8, "2. Oscilloscope.", 0, 24);
    pico_ssd1306::drawText(display, font_5x8, "3. Songs.", 0, 36);
    
    display->sendBuffer();
}

void drawSynthScreen() {
    char buffer[32];
    
    display->clear();
    pico_ssd1306::drawText(display, font_5x8, "Synth Menu:", 0, 0);

    // Display dynamic params with Y positions explicitly set
    int y_pos = 12;
    
    pico_ssd1306::drawText(display, font_5x8, "f:", 0, y_pos);
    sprintf(buffer, "%.0f", fakefreq);
    pico_ssd1306::drawText(display, font_5x8, buffer, 10, y_pos);
    pico_ssd1306::drawText(display, font_5x8, "Hz", 40, y_pos);
    drawPercentageBar(20, 60, y_pos);
    y_pos += 10;

    pico_ssd1306::drawText(display, font_5x8, "g:", 0, y_pos);
    sprintf(buffer, "%.0f", fakefreq);
    pico_ssd1306::drawText(display, font_5x8, buffer, 10, y_pos);
    pico_ssd1306::drawText(display, font_5x8, "Hz", 40, y_pos);
    drawPercentageBar(20, 60, y_pos);
    y_pos += 10;
    
    pico_ssd1306::drawText(display, font_5x8, "A:", 0, y_pos);
    sprintf(buffer, "%.0f", fakefreq);
    pico_ssd1306::drawText(display, font_5x8, buffer, 10, y_pos);
    pico_ssd1306::drawText(display, font_5x8, "ms", 40, y_pos);
    drawPercentageBar(20, 60, y_pos);
    y_pos += 10;

    pico_ssd1306::drawText(display, font_5x8, "D:", 0, y_pos);
    sprintf(buffer, "%.0f", fakefreq);
    pico_ssd1306::drawText(display, font_5x8, buffer, 10, y_pos);
    pico_ssd1306::drawText(display, font_5x8, "ms", 40, y_pos);
    drawPercentageBar(20, 60, y_pos);
    y_pos += 10;

    pico_ssd1306::drawText(display, font_5x8, "S:", 0, y_pos);
    sprintf(buffer, "%.0f", fakefreq);
    pico_ssd1306::drawText(display, font_5x8, buffer, 10, y_pos);
    pico_ssd1306::drawText(display, font_5x8, "%", 40, y_pos);
    drawPercentageBar(20, 60, y_pos);
    y_pos += 10;

    pico_ssd1306::drawText(display, font_5x8, "R:", 0, y_pos);
    sprintf(buffer, "%.0f", fakefreq);
    pico_ssd1306::drawText(display, font_5x8, buffer, 10, y_pos);
    pico_ssd1306::drawText(display, font_5x8, "ms", 40, y_pos);
    drawPercentageBar(20, 60, y_pos);

    display->sendBuffer();  // Update the display
}

void drawOscilloscope() {
    double val = 100;
    int scaledData = val * (SCREEN_HEIGHT - 10) / 1023 + 10;
    
    // Clear previous line by drawing it in SUBTRACT mode
    drawLine(display, itervar - 1, 0, itervar - 1, SCREEN_HEIGHT, pico_ssd1306::WriteMode::SUBTRACT);
    
    // Draw new waveform line in ADD mode
    drawLine(display, itervar - 1, previous_y, itervar, scaledData, pico_ssd1306::WriteMode::ADD);
    
    display->sendBuffer();
    
    if (itervar >= SCREEN_WIDTH) {
        itervar = 0;
        display->clear();
    }

    previous_y = scaledData;
    itervar++;
}

void drawDrumScreen() {
    display->clear();
    pico_ssd1306::drawText(display, font_5x8, "Drum Properties ^ ", 0, 0);
    chooseDrumScreen();

    int y_pos = 20;
    switch(drum_screen_state) {
        case 1:
            pico_ssd1306::drawText(display, font_5x8, "Kick", 0, y_pos);
            pico_ssd1306::drawText(display, font_5x8, "10", 40, y_pos);
            pico_ssd1306::drawText(display, font_5x8, "Hz", 60, y_pos);
            break;
        case 2:
            pico_ssd1306::drawText(display, font_5x8, "Snare", 0, y_pos);
            pico_ssd1306::drawText(display, font_5x8, "10", 40, y_pos);
            pico_ssd1306::drawText(display, font_5x8, "Hz", 60, y_pos);
            break;
        case 3:
            pico_ssd1306::drawText(display, font_5x8, "Tom", 0, y_pos);
            pico_ssd1306::drawText(display, font_5x8, "10", 40, y_pos);
            pico_ssd1306::drawText(display, font_5x8, "Hz", 60, y_pos);
            break;
    }

    display->sendBuffer();
}

void chooseDrumScreen() {
    adc_select_input(1); // Select ADC1 (VRy)
    int Yval = adc_read();

    if(drum_screen_state > MAX_DRUM_SCREENS) { //loop around to main menu
        drum_screen_state = 1; //reset to main menu
        sleep_ms(100);
        display->clear();
    } else if(drum_screen_state < 1) {
        drum_screen_state = MAX_DRUM_SCREENS;
        sleep_ms(100);
        display->clear();
    } else {
        if(Yval > DEAD_ZONE_UPPER) { //otherwise check deadzone
            drum_screen_state++; // go right
            sleep_ms(100);
            display->clear();
        } else if(Yval < DEAD_ZONE_LOWER) {
            drum_screen_state--; //go left
            sleep_ms(100);
            display->clear();
        }

        printf("Drum screen: %d\n", drum_screen_state);
    }
}

void drawSongScreen() {
    char buffer[32];
    
    display->clear();
    pico_ssd1306::drawText(display, font_5x8, "Songs Menu: ^ v", 0, 0);
    
    // Call the function to handle joystick input
    chooseSongScreen();
    
    // Calculate positions for drawing songs
    int yPos = 12; // Starting y position after the header
    int lineHeight = 12; // Height of each song line
    
    // Draw all song options including "None"
    for(int i = 0; i < MAX_SONG_SCREENS; i++) {
        // Add indicator for currently selected song
        if((i + 1 == selected_song) || (i == 3 && selected_song == 0)) {
            pico_ssd1306::drawText(display, font_5x8, "> ", 10, yPos + (i * lineHeight));
        } else {
            pico_ssd1306::drawText(display, font_5x8, "  ", 10, yPos + (i * lineHeight));
        }
        
        pico_ssd1306::drawText(display, font_5x8, i < 3 ? songNames[i] : "None", 25, yPos + (i * lineHeight));
        
        // Draw box around the currently highlighted (not necessarily selected) song
        if(i + 1 == song_screen_state) {
            // Use our custom drawing function
            drawRect(display, 2, yPos + (i * lineHeight) - 2, 
                     124, lineHeight, pico_ssd1306::WriteMode::ADD);
        }
    }
    
    // Display additional info about the highlighted song if it's not "None"
    if(song_screen_state < 4) {
        int info_y = yPos + (MAX_SONG_SCREENS * lineHeight) + 4;
        // <TODO> add an actual BPM
        pico_ssd1306::drawText(display, font_5x8, "BPM: ", 0, info_y);
        sprintf(buffer, "%d", 80 + (song_screen_state * 20));
        pico_ssd1306::drawText(display, font_5x8, buffer, 40, info_y);
    }
    
    // show the currently active song at the bottom
    int bottom_y = SCREEN_HEIGHT - 8;
    if(selected_song > 0) {
        pico_ssd1306::drawText(display, font_5x8, "Playing: ", 0, bottom_y);
        pico_ssd1306::drawText(display, font_5x8, songNames[selected_song - 1], 60, bottom_y);
    } else {
        pico_ssd1306::drawText(display, font_5x8, "No song playing", 0, bottom_y);
    }
    
    display->sendBuffer();
}

void chooseSongScreen() {
    adc_select_input(1); // Select ADC1 (VRy)
    int Yval = adc_read();
    
    adc_select_input(2); // Select ADC2 (SW)
    int button_state = adc_read() > 1000 ? 1 : 0; // Convert analog reading to digital

    if(song_screen_state > MAX_SONG_SCREENS) { // loop around
        song_screen_state = 1;
        sleep_ms(100);
        display->clear();
    } else if(song_screen_state < 1) {
        song_screen_state = MAX_SONG_SCREENS;
        sleep_ms(100);
        display->clear();
    } else {
        if(Yval > DEAD_ZONE_UPPER) { // move down
            song_screen_state++;
            sleep_ms(100);
            display->clear();
        } else if(Yval < DEAD_ZONE_LOWER) { // move up
            song_screen_state--;
            sleep_ms(100);
            display->clear();
        }
    }

    if (button_state == 0 && previous_button_state == 1) {
        // Button was just pressed
        if(song_screen_state == 4) { // "None" option selected
            selected_song = 0;
            printf("No song selected\n");
        } else {
            selected_song = song_screen_state; // Select the current song
            printf("Selected song: %s\n", songNames[selected_song - 1]);
        }
        sleep_ms(200); // Debounce
    }
    
    previous_button_state = button_state; // Update the button state for next iteration
}

void chooseScreen() {
    adc_select_input(0); // Select ADC0 (VRx)
    int Xval = adc_read();

    if(screen_state > MAX_SCREENS) { //loop around to main menu
        screen_state = 1; //reset to main menu
        sleep_ms(100);
        display->clear();
    } else if(screen_state < 1) {
        screen_state = MAX_SCREENS;
        sleep_ms(100);
        display->clear();
    } else {
        if(Xval > DEAD_ZONE_UPPER) { //otherwise check deadzone
            screen_state++; // go right
            sleep_ms(100);
            display->clear();
        } else if(Xval < DEAD_ZONE_LOWER) {
            screen_state--; //go left
            sleep_ms(100);
            display->clear();
        }
    }
}

void drawScreen() {
    switch (screen_state) {
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
        
        default:
            display->clear();
            display->sendBuffer();
            break;
    }
}