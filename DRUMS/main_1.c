#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// Include your sample data headers
#include "kick-16bit.h"
#include "crash-16bit.h"
#include "snare-16bit.h"
#include "tom1-16bit.h"
#include "tom2-16bit.h"

// Include extra audio samples
#include "audio_data.h"
#include "rick_roll.h"

// Helper macros
#define bit_set(var, bitno) ((var) |= 1 << (bitno))
#define bit_clr(var, bitno) ((var) &= ~(1 << (bitno)))
#define testbit(var, bitno) (((var) >> (bitno)) & 0x01)

// Audio settings
#define SAMPLE_RATE 22050
#define PWM_CLOCK_DIV 1.0f
#define PWM_WRAP 4095  // 12-bit resolution for PWM (easier to debug)

// Button settings
#define BTTN_STABLE_TIME_MS 30
#define BTTN_STABLE_CYCLES (((BTTN_STABLE_TIME_MS) * (SAMPLE_RATE)) / 1000)

// Track settings
#define TRACK_NR 6
#define NR_SAMPLES_OF(track) ((sizeof(track)) / (sizeof(int16_t)))
#define MIN_SIGNED_16BIT_VAL -32768
#define MAX_SIGNED_16BIT_VAL 32767

// PWM configuration
#define AUDIO_PIN 0  // GPIO pin 0 for audio output
#define PWM_SLICE_NUM 0  // PWM slice for GPIO 0

// Button pin definitions - MODIFIED to match the requested arrangement
const uint BUTTON_PINS[TRACK_NR] = {16, 17, 18, 19, 28, 7};  // GPIO pins for buttons
// Corresponding sounds: kick, tom1, tom2, snare, crash, (empty slot)

// Loop functionality settings
#define RECORD_PIN 27     // GPIO pin for record mode button
#define PLAY_PIN 26       // GPIO pin for play loop button
#define CLEAR_PIN 22      // GPIO pin for clear loop button
#define BEAT_SELECT_PIN 21 // GPIO pin for classic beat selection
#define MAX_LOOP_EVENTS 1000  // Maximum number of events in the loop

// Sound selection pin
#define SOUND_SELECT_PIN 20  // GPIO pin for sound selection mode
#define SOUND_LIBRARY_SIZE 7  // Number of available sounds

// LED indicators
#define RECORD_LED_PIN 15  // GPIO pin for recording status LED
#define PLAY_LED_PIN 14    // GPIO pin for playback status LED
#define LED_FLASH_PERIOD 250  // LED flash period in ms

// Classic beat patterns
#define NUM_CLASSIC_BEATS 3   // Number of built-in classic beats
#define MAX_BEAT_EVENTS 50    // Maximum number of events in a classic beat

// Global variables for audio
volatile uint32_t tracks_playing = 0;  // each bit tells us if a track is playing
volatile uint32_t button_tracker[TRACK_NR] = {0};
volatile int32_t samp_play_cnt[TRACK_NR] = {0};
volatile bool buttons_pressed[TRACK_NR] = {0};
volatile bool button_state_changed = false;

// Loop control globals
volatile bool record_mode = false;
volatile bool play_mode = false;
volatile uint64_t loop_timestamp = 0;  // Current timestamp for loop playback
volatile uint64_t loop_start_time = 0; // When recording started
volatile uint64_t loop_end_time = 0;   // When recording ended
volatile uint64_t loop_duration = 0;   // Total duration of the loop

// Sound selection mode control
volatile bool sound_select_mode = false;
volatile uint8_t current_button_to_configure = 0;  // Which button is being configured
volatile uint8_t currently_selected_sound = 0;     // Which sound is currently selected

// Classic beats globals
volatile uint8_t current_beat = 2;     // Currently selected classic beat (start with funk beat)
volatile bool classic_beat_mode = false; // Whether classic beat mode is active

// LED status variables - no longer need these for flashing since LEDs stay solid
// Keeping variable declarations for compatibility with other parts of the code
volatile uint32_t record_led_state = 0;
volatile uint32_t play_led_state = 0;
volatile uint32_t led_flash_timestamp = 0;

// Loop event structure
typedef struct {
    uint8_t track;      // Which drum sound to play
    uint64_t timestamp; // Timestamp when it should play
} LoopEvent;

// Loop storage
volatile LoopEvent loop_events[MAX_LOOP_EVENTS];
volatile uint16_t loop_event_count = 0;

// Classic beat patterns
// Each beat is defined as a series of {track, time_ms} pairs
// MODIFIED: Updated the track indices to match new arrangement
// 0=kick, 1=tom1, 2=tom2, 3=snare, 4=crash
const LoopEvent classic_beats[NUM_CLASSIC_BEATS][MAX_BEAT_EVENTS] = {
    // Beat 0: Billie Jean beat
    {
        {4, 0},      // Crash/Hi-hat on beat 1
        {0, 0},      // Kick on beat 1
        {4, 250},    // Crash/Hi-hat on the "and" of 1
        {4, 500},    // Crash/Hi-hat on beat 2
        {3, 500},    // Snare on beat 2
        {4, 750},    // Crash/Hi-hat on the "and" of 2
        {4, 1000},   // Crash/Hi-hat on beat 3
        {0, 1000},   // Kick on beat 3
        {4, 1250},   // Crash/Hi-hat on the "and" of 3
        {4, 1500},   // Crash/Hi-hat on beat 4
        {3, 1500},   // Snare on beat 4
        {4, 1750},   // Crash/Hi-hat on the "and" of 4
        {0, 2000},   // Loop back to beat 1
        {255, 0}     // End marker (track 255 means end of pattern)
    },
    
    // Beat 1: Simple hip-hop beat
    {
        {0, 0},      // Kick on beat 1
        {3, 500},    // Snare on beat 2
        {0, 750},    // Kick on "and" of 2
        {0, 1000},   // Kick on beat 3
        {3, 1500},   // Snare on beat 4
        {4, 1750},   // Crash/Hi-hat accent
        {0, 2000},   // Loop back to beat 1
        {255, 0}     // End marker
    },
    
    // Beat 2: Funk beat
    {
        {0, 0},      // Kick on beat 1
        {4, 125},    // Crash/Hi-hat on 16th note
        {3, 250},    // Snare on the "and" of 1
        {4, 375},    // Crash/Hi-hat on 16th note
        {0, 500},    // Kick on beat 2
        {4, 625},    // Crash/Hi-hat on 16th note
        {3, 750},    // Snare on the "and" of 2
        {0, 875},    // Kick on 16th note
        {4, 1000},   // Crash/Hi-hat on beat 3
        {3, 1250},   // Snare on the "and" of 3
        {0, 1500},   // Kick on beat 4
        {4, 1625},   // Crash/Hi-hat on 16th note
        {3, 1750},   // Snare on the "and" of 4
        {4, 1875},   // Crash/Hi-hat on 16th note
        {0, 2000},   // Loop back to beat 1
        {255, 0}     // End marker
    }
};

// Length of each classic beat in milliseconds
const uint64_t classic_beat_durations[NUM_CLASSIC_BEATS] = {2000, 2000, 2000};

// Array of all available sounds
// This array contains all sound samples that can be assigned to buttons
const uint32_t available_sounds_sizes[SOUND_LIBRARY_SIZE] = {
    NR_SAMPLES_OF(kick),
    NR_SAMPLES_OF(tom1),
    NR_SAMPLES_OF(tom2),
    NR_SAMPLES_OF(snare),
    NR_SAMPLES_OF(crash),
    NR_SAMPLES_OF(piano_loop_data),
    NR_SAMPLES_OF(rick_roll_audio)
};

const int16_t *available_sounds[SOUND_LIBRARY_SIZE] = {
    kick,
    tom1,
    tom2,
    snare,
    crash,
    piano_loop_data,
    rick_roll_audio
};

const char *sound_names[SOUND_LIBRARY_SIZE] = {
    "Kick",
    "Tom1",
    "Tom2",
    "Snare",
    "Crash",
    "Audio",
    "Rick Roll"
};

// Dynamic mapping of buttons to sounds
// These arrays will be modified during runtime when in sound selection mode
volatile uint32_t tracks_samp_nr[TRACK_NR] = {
    NR_SAMPLES_OF(kick),   // GPIO 16: kick (default)
    NR_SAMPLES_OF(tom1),   // GPIO 17: tom1 (default)
    NR_SAMPLES_OF(tom2),   // GPIO 18: tom2 (default)
    NR_SAMPLES_OF(snare),  // GPIO 19: snare (default)
    NR_SAMPLES_OF(crash),  // GPIO 28: crash (default)
    0                      // GPIO 7: Empty slot
};

volatile int16_t const *tracks[TRACK_NR] = {
    kick,      // GPIO 16: kick (default)
    tom1,      // GPIO 17: tom1 (default)
    tom2,      // GPIO 18: tom2 (default)
    snare,     // GPIO 19: snare (default)
    crash,     // GPIO 28: crash (default)
    NULL       // GPIO 7: Empty slot
};

// Button to sound mapping - keep track of which sound is assigned to each button
volatile uint8_t button_sound_mapping[TRACK_NR] = {0, 1, 2, 3, 4, 5};

// Function to set up PWM for audio output
void pwm_audio_init(void) {
    // Configure GPIO0 for PWM
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    
    // Get PWM slice for GPIO0
    uint slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    
    // Configure PWM
    // System clock is 125MHz by default
    float pwm_clock = clock_get_hz(clk_sys);  // typically 125MHz
    float divider = pwm_clock / (SAMPLE_RATE * PWM_WRAP);
    
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, divider);
    pwm_config_set_wrap(&config, PWM_WRAP - 1);
    pwm_init(slice_num, &config, true);
    
    // Set initial PWM level to middle (silence)
    pwm_set_gpio_level(AUDIO_PIN, PWM_WRAP / 2);
    
    printf("PWM Audio initialized. Clock: %.1f MHz, Divider: %.3f\n", 
           pwm_clock / 1000000, divider);
}

// Debug function to print button states
void print_button_states() {
    printf("Button states: ");
    for (int i = 0; i < TRACK_NR; i++) {
        printf("%d:%d ", i, !gpio_get(BUTTON_PINS[i]));
    }
    printf("\n");
}

// Print loop status
void print_loop_status() {
    printf("Loop status: Record=%d Play=%d ClassicBeat=%d Events=%d Duration=%llu ms\n", 
           record_mode, play_mode, classic_beat_mode ? current_beat : -1, 
           loop_event_count, loop_duration / 1000);
}

// Print sound configuration
void print_sound_config() {
    printf("Current sound configuration:\n");
    for (int i = 0; i < TRACK_NR; i++) {
        if (button_sound_mapping[i] < SOUND_LIBRARY_SIZE) {
            printf("- GPIO %d: %s\n", BUTTON_PINS[i], sound_names[button_sound_mapping[i]]);
        } else {
            printf("- GPIO %d: Empty\n", BUTTON_PINS[i]);
        }
    }
}

// Change a button's assigned sound
void change_button_sound(uint8_t button_idx, uint8_t sound_idx) {
    if (button_idx >= TRACK_NR || sound_idx >= SOUND_LIBRARY_SIZE) {
        return;
    }
    
    // Update the mapping
    button_sound_mapping[button_idx] = sound_idx;
    
    // Update the sample data
    tracks_samp_nr[button_idx] = available_sounds_sizes[sound_idx];
    tracks[button_idx] = available_sounds[sound_idx];
    
    printf("Changed GPIO %d to play %s\n", BUTTON_PINS[button_idx], sound_names[sound_idx]);
}

// Add an event to the loop
void add_loop_event(uint8_t track) {
    if (loop_event_count < MAX_LOOP_EVENTS) {
        uint64_t current_time = time_us_64();
        uint64_t relative_time = current_time - loop_start_time;
        
        loop_events[loop_event_count].track = track;
        loop_events[loop_event_count].timestamp = relative_time;
        loop_event_count++;
        printf("Added event: Track %d at time %llu ms\n", track, relative_time / 1000);
    } else {
        printf("Loop event buffer full!\n");
    }
}

// Clear all loop events
void clear_loop() {
    loop_event_count = 0;
    loop_duration = 0;
    printf("Loop cleared\n");
}

// Load a classic beat pattern into the loop
void load_classic_beat(uint8_t beat_index) {
    if (beat_index >= NUM_CLASSIC_BEATS) {
        printf("Invalid beat index: %d\n", beat_index);
        return;
    }
    
    // Clear any existing loop
    loop_event_count = 0;
    
    // Copy the classic beat pattern to the loop events
    int i = 0;
    while (i < MAX_BEAT_EVENTS && classic_beats[beat_index][i].track != 255) {
        loop_events[i].track = classic_beats[beat_index][i].track;
        loop_events[i].timestamp = classic_beats[beat_index][i].timestamp * 1000; // Convert ms to μs
        i++;
    }
    
    loop_event_count = i;
    loop_duration = classic_beat_durations[beat_index] * 1000; // Convert ms to μs
    
    printf("Loaded classic beat %d with %d events, duration: %llu ms\n", 
           beat_index, loop_event_count, loop_duration / 1000);
}

// GPIO interrupt handler for buttons
void gpio_callback(uint gpio, uint32_t events) {
    // Only respond to falling edge events (button press, not release)
    if (!(events & GPIO_IRQ_EDGE_FALL)) {
        return; // Ignore rising edge events (button release)
    }
    
    // Sound selection mode button
    if (gpio == SOUND_SELECT_PIN) {
        if (!gpio_get(SOUND_SELECT_PIN)) {  // Button pressed (active low)
            sound_select_mode = !sound_select_mode;
            
            if (sound_select_mode) {
                printf("SOUND SELECTION mode ON\n");
                printf("Press any drum button to select it, then press again to cycle through sounds\n");
                print_sound_config();
                
                // Reset selection state
                current_button_to_configure = 0xFF; // No button selected yet
                currently_selected_sound = 0;
                
                // Turn off other modes
                record_mode = false;
                play_mode = false;
                classic_beat_mode = false;
                
                // Turn off mode LEDs
                gpio_put(RECORD_LED_PIN, 0);
                gpio_put(PLAY_LED_PIN, 0);
            } else {
                printf("SOUND SELECTION mode OFF\n");
                print_sound_config();
            }
        }
        return;
    }
    
    // Mode control buttons
    if (gpio == RECORD_PIN) {
        if (!gpio_get(RECORD_PIN)) {  // Button pressed (active low)
            // Only respond if not in sound selection mode
            if (!sound_select_mode) {
                if (!record_mode) {
                    // Start recording
                    record_mode = true;
                    classic_beat_mode = false; // Exit classic beat mode if active
                    loop_start_time = time_us_64();
                    loop_event_count = 0; // Clear previous events
                    printf("RECORD mode ON - started at %llu\n", loop_start_time);
                    gpio_put(RECORD_LED_PIN, 1); // Turn on record LED
                } else {
                    // Stop recording
                    record_mode = false;
                    loop_end_time = time_us_64();
                    loop_duration = loop_end_time - loop_start_time;
                    printf("RECORD mode OFF - duration: %llu ms\n", loop_duration / 1000);
                    gpio_put(RECORD_LED_PIN, 0); // Turn off record LED
                    
                    // Automatically start playback
                    if (loop_event_count > 0) {
                        play_mode = true;
                        loop_timestamp = 0;
                        printf("PLAY mode ON - auto-starting playback\n");
                        gpio_put(PLAY_LED_PIN, 1); // Turn on play LED
                    }
                }
            }
        }
        return;
    }
    
    if (gpio == PLAY_PIN) {
        if (!gpio_get(PLAY_PIN)) {  // Button pressed (active low)
            // Only respond if not in sound selection mode
            if (!sound_select_mode) {
                play_mode = !play_mode;
                if (play_mode) {
                    printf("PLAY mode ON\n");
                    // Reset loop position when starting playback
                    loop_timestamp = 0;
                    gpio_put(PLAY_LED_PIN, 1); // Turn on play LED
                } else {
                    printf("PLAY mode OFF\n");
                    gpio_put(PLAY_LED_PIN, 0); // Turn off play LED
                }
            }
        }
        return;
    }
    
    if (gpio == CLEAR_PIN) {
        if (!gpio_get(CLEAR_PIN)) {  // Button pressed (active low)
            // Only respond if not in sound selection mode
            if (!sound_select_mode) {
                clear_loop();
                classic_beat_mode = false;
            }
        }
        return;
    }
    
    if (gpio == BEAT_SELECT_PIN) {
        if (!gpio_get(BEAT_SELECT_PIN)) {  // Button pressed (active low)
            // Only respond if not in sound selection mode
            if (!sound_select_mode) {
                // Cycle through classic beats
                current_beat = (current_beat + 1) % NUM_CLASSIC_BEATS;
                
                // Load the selected beat
                load_classic_beat(current_beat);
                
                // Turn on classic beat mode and start playback
                classic_beat_mode = true;
                play_mode = true;
                record_mode = false;
                loop_timestamp = 0;
                
                // Update LEDs
                gpio_put(RECORD_LED_PIN, 0);
                gpio_put(PLAY_LED_PIN, 1);
                
                printf("Classic beat %d selected and playing\n", current_beat);
            }
        }
        return;
    }
    
    // Drum buttons
    int button_idx = -1;
    
    // Find which button was pressed
    for (int i = 0; i < TRACK_NR; i++) {
        if (gpio == BUTTON_PINS[i]) {
            button_idx = i;
            break;
        }
    }
    
    if (button_idx >= 0) {
        // Button state changed
        buttons_pressed[button_idx] = !gpio_get(BUTTON_PINS[button_idx]);
        button_state_changed = true;
        
        // If in sound selection mode, configure the button
        if (sound_select_mode && buttons_pressed[button_idx]) {
            if (current_button_to_configure == button_idx) {
                // Button already selected, cycle through available sounds
                currently_selected_sound = (currently_selected_sound + 1) % SOUND_LIBRARY_SIZE;
                change_button_sound(button_idx, currently_selected_sound);
                
                // Play the selected sound for preview
                bit_set(tracks_playing, button_idx);
                samp_play_cnt[button_idx] = tracks_samp_nr[button_idx];
            } else {
                // New button selected, prepare to configure it
                current_button_to_configure = button_idx;
                currently_selected_sound = button_sound_mapping[button_idx];
                printf("Selected GPIO %d (currently plays %s)\n", 
                       BUTTON_PINS[button_idx], 
                       sound_names[currently_selected_sound]);
                printf("Press again to cycle through available sounds\n");
                
                // Play the current sound for preview
                bit_set(tracks_playing, button_idx);
                samp_play_cnt[button_idx] = tracks_samp_nr[button_idx];
            }
            return;
        }
        
        // Normal drum button handling (play sound)
        // Check button state - only respond to button presses (not releases)
        if (buttons_pressed[button_idx]) {
            printf("Button %d pressed (GPIO %d)\n", button_idx, BUTTON_PINS[button_idx]);
            
            // Start playing the sample
            bit_set(tracks_playing, button_idx);
            samp_play_cnt[button_idx] = tracks_samp_nr[button_idx];
            
            // If in record mode, store the event
            if (record_mode) {
                add_loop_event(button_idx);
            }
        }
    }
}

// Check for loop events to play
void check_loop_events() {
    // Process loop events if in play mode
    if (play_mode && loop_duration > 0) {
        for (int i = 0; i < loop_event_count; i++) {
            // Check if it's time to play this event
            // We use a small window to catch events that might be just slightly off due to timing
            if (loop_events[i].timestamp <= loop_timestamp && 
                loop_events[i].timestamp > loop_timestamp - 5000) { // 5ms window
                
                // Trigger sample from the loop
                uint8_t track = loop_events[i].track;
                if (track < TRACK_NR) {
                    bit_set(tracks_playing, track);
                    samp_play_cnt[track] = tracks_samp_nr[track];
                    printf("Loop triggered track %d at time %llu ms\n", track, loop_timestamp / 1000);
                }
            }
        }
        
        // Update loop position and check for loop end
        if (loop_timestamp >= loop_duration) {
            loop_timestamp = 0; // Loop around
            printf("Loop wrapped around\n");
        }
    }
}

// Update LED states based on mode - LEDs now stay solid instead of flashing
void update_led_states() {
    // Set LEDs to solid on/off based on mode
    if (record_mode) {
        gpio_put(RECORD_LED_PIN, 1); // Solid ON when recording
    } else {
        gpio_put(RECORD_LED_PIN, 0); // OFF when not recording
    }
    
    if (play_mode) {
        gpio_put(PLAY_LED_PIN, 1); // Solid ON when playing
    } else {
        gpio_put(PLAY_LED_PIN, 0); // OFF when not playing
    }
}

// Timer callback function for loop timing
bool loop_timer_callback(struct repeating_timer *t) {
    // Update loop timestamp
    if (play_mode && loop_duration > 0) {
        loop_timestamp += 1000; // Increment by 1ms (timer is set to 1ms)
        check_loop_events();
    }
    
    // Update LED states (now just keeps LEDs solidly on/off)
    update_led_states();
    
    return true;
}

// Timer callback function for audio sample generation
bool sample_timer_callback(struct repeating_timer *t) {
    int32_t mix = 0;
    int32_t samp_sum = 0;
    uint16_t pwm_val = 0;
    
    // Process active tracks and mix samples
    for (int i = 0; i < TRACK_NR; i++) {
        if (testbit(tracks_playing, i) == 1) {
            uint32_t current_samp_nr = tracks_samp_nr[i] - samp_play_cnt[i];
            if (current_samp_nr < tracks_samp_nr[i] && tracks[i] != NULL) {  // Bounds check and ensure track exists
                samp_sum += tracks[i][current_samp_nr];
                samp_play_cnt[i]--;  // Decrement number of samples left to play
                
                if (samp_play_cnt[i] <= 0) {
                    bit_clr(tracks_playing, i);  // Finished playing this track
                }
            } else {
                // Index out of bounds, stop playing this track
                bit_clr(tracks_playing, i);
            }
        }
    }
    
    // Apply clipping
    mix = samp_sum;
    if (mix < MIN_SIGNED_16BIT_VAL) {
        mix = MIN_SIGNED_16BIT_VAL;
    } else if (mix > MAX_SIGNED_16BIT_VAL) {
        mix = MAX_SIGNED_16BIT_VAL;
    }
    
    // Convert from signed 16-bit to PWM value (0 to PWM_WRAP)
    // Map from [-32768, 32767] to [0, PWM_WRAP]
    pwm_val = (uint16_t)(((int32_t)mix - MIN_SIGNED_16BIT_VAL) * PWM_WRAP / 65536);
    
    // Output to PWM
    pwm_set_gpio_level(AUDIO_PIN, pwm_val);
    
    return true;
}

int main() {
    // Initialize standard library
    stdio_init_all();
    
    // Wait for USB connection to be established to see debug messages
    sleep_ms(2000);
    
    printf("Pico Drum Machine with Sound Selection Mode starting...\n");
    printf("Default pin layout: GPIO 16=kick, 17=tom1, 18=tom2, 19=snare, 28=crash\n");
    printf("You can now dynamically reassign sounds to buttons during runtime!\n");
    
    // Initialize PWM for audio output
    pwm_audio_init();
    
    // Initialize drum button GPIOs with interrupt
    for (int i = 0; i < TRACK_NR; i++) {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);  // Enable pull-up resistors
        
        // Set up interrupt for both rising and falling edges
        gpio_set_irq_enabled_with_callback(BUTTON_PINS[i], 
                                          GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                          true, &gpio_callback);
    }
    
    // Initialize mode control buttons
    gpio_init(RECORD_PIN);
    gpio_set_dir(RECORD_PIN, GPIO_IN);
    gpio_pull_up(RECORD_PIN);
    gpio_set_irq_enabled(RECORD_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    
    gpio_init(PLAY_PIN);
    gpio_set_dir(PLAY_PIN, GPIO_IN);
    gpio_pull_up(PLAY_PIN);
    gpio_set_irq_enabled(PLAY_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    
    gpio_init(CLEAR_PIN);
    gpio_set_dir(CLEAR_PIN, GPIO_IN);
    gpio_pull_up(CLEAR_PIN);
    gpio_set_irq_enabled(CLEAR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    
    gpio_init(BEAT_SELECT_PIN);
    gpio_set_dir(BEAT_SELECT_PIN, GPIO_IN);
    gpio_pull_up(BEAT_SELECT_PIN);
    gpio_set_irq_enabled(BEAT_SELECT_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    
    // Initialize sound selection mode button
    gpio_init(SOUND_SELECT_PIN);
    gpio_set_dir(SOUND_SELECT_PIN, GPIO_IN);
    gpio_pull_up(SOUND_SELECT_PIN);
    gpio_set_irq_enabled(SOUND_SELECT_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    
    // Initialize LED indicator pins
    gpio_init(RECORD_LED_PIN);
    gpio_set_dir(RECORD_LED_PIN, GPIO_OUT);
    gpio_put(RECORD_LED_PIN, 0);  // Start with LED off
    
    gpio_init(PLAY_LED_PIN);
    gpio_set_dir(PLAY_LED_PIN, GPIO_OUT);
    gpio_put(PLAY_LED_PIN, 0);  // Start with LED off
    
    // Configure repeating timer for audio sample generation
    struct repeating_timer sample_timer;
    add_repeating_timer_us(-1000000 / SAMPLE_RATE, sample_timer_callback, NULL, &sample_timer);
    
    // Configure repeating timer for loop timing with 1ms precision
    struct repeating_timer loop_timer;
    add_repeating_timer_ms(-1, loop_timer_callback, NULL, &loop_timer);
    
    printf("Pico Drum Machine running!\n");
    printf("Pins configured:\n");
    printf("- Audio: GPIO %d\n", AUDIO_PIN);
    printf("- Drum buttons:\n");
    printf("  - GPIO 16: Kick (default)\n");
    printf("  - GPIO 17: Tom1 (default)\n");
    printf("  - GPIO 18: Tom2 (default)\n");
    printf("  - GPIO 19: Snare (default)\n");
    printf("  - GPIO 28: Crash (default)\n");
    printf("  - GPIO 7: Empty slot\n");
    printf("- Record mode: GPIO %d (LED: GPIO %d)\n", RECORD_PIN, RECORD_LED_PIN);
    printf("- Play mode: GPIO %d (LED: GPIO %d)\n", PLAY_PIN, PLAY_LED_PIN);
    printf("- Clear loop: GPIO %d\n", CLEAR_PIN);
    printf("- Classic beat selection: GPIO %d\n", BEAT_SELECT_PIN);
    printf("- Sound selection mode: GPIO %d\n", SOUND_SELECT_PIN);
    printf("- %d classic beats available\n", NUM_CLASSIC_BEATS);
    printf("- %d sounds available to assign\n", SOUND_LIBRARY_SIZE);
    printf("Timing precision: 1ms\n");
    printf("\n");
    printf("To change a sound assignment:\n");
    printf("1. Press GPIO %d to enter Sound Selection Mode\n", SOUND_SELECT_PIN);
    printf("2. Press a drum button to select it\n");
    printf("3. Press the same button again to cycle through available sounds\n");
    printf("4. Press GPIO %d again to exit Sound Selection Mode\n", SOUND_SELECT_PIN);
    printf("\n");
    print_sound_config();
    
    // Main loop
    while (1) {
        sleep_ms(500);
        
        // Print status updates
        if (button_state_changed) {
            print_button_states();
            button_state_changed = false;
        }
        
        // Periodically print loop status
        static uint32_t last_status_time = 0;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_status_time > 5000) {  // Every 5 seconds
            print_loop_status();
            if (sound_select_mode) {
                printf("Currently in Sound Selection Mode\n");
                if (current_button_to_configure != 0xFF) {
                    printf("- Configuring GPIO %d, current sound: %s\n", 
                           BUTTON_PINS[current_button_to_configure], 
                           sound_names[currently_selected_sound]);
                }
            }
            last_status_time = now;
        }
    }
    
    return 0;
}