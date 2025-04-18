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
//#include "audio_data.h"
#include "rick_roll.h"
// Add the new sound header files
#include "darude_sandstorm.h"
#include "blk_enter_dragon.h"


void bit_set(volatile uint32_t *track_bitmap, uint8_t tracknumber) {
    // OR the track bit position with 1 to set it
    *track_bitmap |= 1 << tracknumber;
}

void bit_clr(volatile uint32_t *track_bitmap, uint8_t tracknumber) {
    // AND the track bit position with 0 to clear it
    *track_bitmap &= ~(1 << tracknumber);
}

uint8_t testbit(uint32_t track_bitmap, uint8_t tracknumber) {
    // shift everything right by our track nunber and chekc its value
    // AND with the mask 
    return (track_bitmap >> tracknumber) & 0x01;
}

#define SAMPLE_RATE 22050  // 22Khz, to be fair we got this from ur mans repo, 
#define PWM_WRAP 4095   // 12 bits for PWM, 16 didnt really work

#define num_active_tracks 5
// this just help to 
#define NR_SAMPLES_OF(track) ((sizeof(track)) / (sizeof(int16_t)))



// PWM configuration
const int pwm_output_pin  = 0;       // we shall use the gpio pin 0 for outputting the PWM
// https://electronics.stackexchange.com/questions/729277/what-is-slicing-in-pwm
// we need to set the slice controlling our pwm, more info here ^^^^

// Capacitave touch pads 
const uint Drum_Pads[num_active_tracks] = {16, 17, 18, 19, 28};  

// pins for the looping 
#define RECORD_PIN 27     
#define PLAY_PIN 12       
#define CLEAR_PIN 22      
#define BEAT_SELECT_PIN 21
#define MAX_LOOP_EVENTS 50 

// Ruari's Arduino will send digital signals here to say which sound we want to play
#define CLASSIC_1 8      
#define CLASSIC_2 9      
#define CLASSIC_3 10     
#define NO_BEAT 11  
#define NUM_CLASSIC_BEATS 3   // Number of built-in classic beats
#define MAX_CLASSIC_BEAT_EVENTS 50    // Maximum number of events in a classic beat
// initialising classic beat, idk if these are still needed but im slightly worried about initialisation
volatile uint8_t current_beat = 2;     // Currently selected classic beat (start with funk beat)
volatile bool classic_beat_mode = false; // Whether classic beat mode is active

// pins for sound config 
#define SOUND_SELECT_PIN 20  // pushbutton for sound config
#define total_num_tracks 9  

// LED indicators for whatever mode we are in 
#define RECORD_LED 15  // GPIO pin for recording status LED
#define PLAY_LED 14    // GPIO pin for playback status LED
#define LED_FLASH_PERIOD 250  // LED flash period in ms

// THESE ALL NEED TO BE VOLTILE. we change stuff with interupts
volatile uint32_t tracks_playing = 0;  // each bit tells us if a track is playing
volatile int32_t samples_left_to_play[num_active_tracks] = {0};

// Loop control globals
volatile bool record_mode = false;
volatile bool play_mode = false;
// millisecond variables for looping
volatile uint64_t loop_timestamp = 0;  // current timestamp for loop
volatile uint64_t loop_start_time = 0; // when we started recoding 
volatile uint64_t loop_end_time = 0;   // when we stopped recording
volatile uint64_t loop_duration = 0;   // total loop time in milliseconds

// Sound selection mode control
volatile bool sound_select_mode = false;
volatile uint8_t current_button_to_configure = 0;  // Which button is being configured
volatile uint8_t currently_selected_sound = 0;     // Which sound is currently selected
volatile uint8_t last_selected_beat = 0xFF; // Track the last selected beat to detect changes,set this as default

// LED status variables - no longer need these for flashing since LEDs stay solid
// Keeping variable declarations for compatibility with other parts of the code
volatile uint32_t record_led_state = 0;
volatile uint32_t play_led_state = 0;
volatile uint32_t led_flash_timestamp = 0;

// for each "loop event" we have a tuple basically with the type of beat and then the time it should plauy
typedef struct {
    uint8_t track;      // the type of drum we should play
    uint64_t timestamp; // Timestamp when it should play
} LoopEvent;

volatile LoopEvent loop_events[MAX_LOOP_EVENTS];
volatile uint16_t loop_event_count = 0;

// Classic beat patterns
// Each beat is defined as an array of LoopEvents,
// each beat has a row, its a matrix
const LoopEvent classic_beats[NUM_CLASSIC_BEATS][MAX_CLASSIC_BEAT_EVENTS] = {

    // WE HAVE NO HIHAT, hithats seem to come up a lot, but we dont have one smh
    // money beat, 
    {
        {4, 0},      // Crash on beat 1
        {0, 0},      // Kick on beat 1
        {4, 250},    // Crash on offbeat
        {4, 500},    // Crash on beat 2
        {3, 500},    // Snare on beat 2
        {4, 750},    // crash on the offbeat again
        {4, 1000},   // Crash on beat 3
        {0, 1000},   // Kick on beat 3
        {4, 1250},   // Crash on offbeat
        {4, 1500},   // Crash beat 4
        {3, 1500},   // Snare beat 4
        {4, 1750},   // Crash on the offbeat
        {0, 2000},   // Loop back to beat 1
        {255, 0}     // 255 MEANS END OF LOOP LETS DO IT AGAIN
    },
    
    // hip hope r smt
    {
        {0, 0},      // Kick on beat 1
        {3, 500},    // Snare on beat 2
        {0, 750},    // Kick on offbeat
        {0, 1000},   // Kick on beat 3
        {3, 1500},   // Snare on beat 4
        {4, 1750},   // Crash on the offbeat ish (could remove this it sounds kinda bad)
        {0, 2000},   // Loop back to beat 1
        {255, 0}     // End marker
    },
    
    // funky beat
    // NOW THIS IS GOOD
    {
        {0, 0},      // Kick on beat 1
        {4, 125},    // Crash on on 1/16 (called semiquavers apparenlty)
        {3, 250},    // Snare on offbeat
        {4, 375},    // Crash 
        {0, 500},    // Kick on beat 2
        {4, 625},    // Crash on 16
        {3, 750},    // Snare on the "and" of 2
        {0, 875},    // Kick on 16th 
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
// Updated to include the new sound samples
const uint32_t available_sounds_sizes[total_num_tracks] = {
    NR_SAMPLES_OF(kick),
    NR_SAMPLES_OF(tom1),
    NR_SAMPLES_OF(tom2),
    NR_SAMPLES_OF(snare),
    NR_SAMPLES_OF(crash),
    NR_SAMPLES_OF(rick_roll_audio),
    NR_SAMPLES_OF(darude_sandstorm_audio),
    NR_SAMPLES_OF(blk_enter_dragon_audio)
};

const int16_t *available_sounds[total_num_tracks] = {
    kick,
    tom1,
    tom2,
    snare,
    crash,
    rick_roll_audio,
    darude_sandstorm_audio,
    blk_enter_dragon_audio
};

const char *sound_names[total_num_tracks] = {
    "Kick",
    "Tom1",
    "Tom2",
    "Snare",
    "Crash",
    "Audio",
    "Rick Roll",
    "Sandstorm",
    "Enter Dragon"
};

// THIS ARRAY IS VERY IMPORTANT
// it hols the sound that the pads are currently assigned,
// this is the configuration when the pico starts up
volatile uint32_t total_samples[num_active_tracks] = {
    NR_SAMPLES_OF(kick),   
    NR_SAMPLES_OF(tom1),   
    NR_SAMPLES_OF(tom2),   
    NR_SAMPLES_OF(snare),  
    NR_SAMPLES_OF(crash),  
};

// just set the initial values to the NORMAL drum sounds
volatile int16_t const *tracks[num_active_tracks] = {
    kick,      
    tom1,      
    tom2,      
    snare,     
    crash,     
};

const uint pusshbuttons[] = {
    SOUND_SELECT_PIN,
    RECORD_PIN,
    PLAY_PIN,
    CLEAR_PIN,
    BEAT_SELECT_PIN
};

const uint beat_pins[] = {
    CLASSIC_1,
    CLASSIC_2,
    CLASSIC_3,
    NO_BEAT
};

// Button to sound mapping - keep track of which sound is assigned to each button
volatile uint8_t button_sound_mapping[num_active_tracks] = {0, 1, 2, 3, 4, 5};

// Function to set up PWM for audio output
void pwm_audio_init(void) {

    // set the pin 0 to be able ot do PWM
    gpio_set_function(pwm_output_pin, GPIO_FUNC_PWM);
    
    uint slice_num = pwm_gpio_to_slice_num(pwm_output_pin);
    
    float pico_clk = clock_get_hz(clk_sys);  
    float actual_pwm_clk = pico_clk / (SAMPLE_RATE * PWM_WRAP);
    
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, actual_pwm_clk);
    pwm_config_set_wrap(&config, PWM_WRAP - 1);
    pwm_init(slice_num, &config, true);
    
    // Set initial PWM level to middle (silence)
    pwm_set_gpio_level(pwm_output_pin, PWM_WRAP / 2);
    
}

// Add an event to the loop
void add_loop_event(uint8_t track) {
    if (loop_event_count < MAX_LOOP_EVENTS) {
        uint64_t current_time = time_us_64();
        uint64_t triggered_time = current_time - loop_start_time;
        
        loop_events[loop_event_count].track = track;
        loop_events[loop_event_count].timestamp = triggered_time;
        loop_event_count++;
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
    while (i < MAX_CLASSIC_BEAT_EVENTS && classic_beats[beat_index][i].track != 255) {
        loop_events[i].track = classic_beats[beat_index][i].track;
        loop_events[i].timestamp = classic_beats[beat_index][i].timestamp * 1000; // Convert ms to μs
        i++;
    }
    
    loop_event_count = i;
    loop_duration = classic_beat_durations[beat_index] * 1000; // Convert ms to μs
    
    printf("Loaded classic beat %d with %d events, duration: %llu ms\n", 
           beat_index, loop_event_count, loop_duration / 1000);
}

// NEW: Check beat selection pins and update beat selection if needed
void check_beat_selection_pins() {
    // Read the state of the beat selection pins
    bool beat1_active = gpio_get(CLASSIC_1);
    bool beat2_active = gpio_get(CLASSIC_2);
    bool beat3_active = gpio_get(CLASSIC_3);
    bool no_beat_active = gpio_get(NO_BEAT);
    
    uint8_t selected_beat = 0xFF;  // Default to no selection
    
    if (beat1_active) {
        selected_beat = 0;  
    } else if (beat2_active) {
        selected_beat = 1;  
    } else if (beat3_active) {
        selected_beat = 2;  
    } else if (no_beat_active) {
        selected_beat = 0xFF; 
    }
    
    // If selection changed and not in sound selection mode
    if (selected_beat != last_selected_beat && !sound_select_mode) {
        last_selected_beat = selected_beat;
        
        if (selected_beat < NUM_CLASSIC_BEATS) {
            // Load and play the selected beat
            current_beat = selected_beat;
            load_classic_beat(current_beat);
            
            // Turn on classic beat mode and start playback
            classic_beat_mode = true;
            play_mode = true;
            record_mode = false;
            loop_timestamp = 0;
            
            // Update LEDs
            gpio_put(RECORD_LED, 0);
            gpio_put(PLAY_LED, 1);
            
            printf("Classic beat %d selected via GPIO pins and playing\n", current_beat);
        } else if (selected_beat == 0xFF) {
            // Stop playing the beat
            play_mode = false;
            classic_beat_mode = false;
            gpio_put(PLAY_LED, 0);
            printf("Beat playback stopped via GPIO pins\n");
        }
    }
}

// Universal GPIO callback for all inputs
void gpio_isr(uint gpio, uint32_t events) {
    // Check if this is a touch sensor
    bool is_touch_sensor = false;
    int touched_pad = -1;
    
    for (int i = 0; i < num_active_tracks; i++) {
        if (gpio == Drum_Pads[i]) {
            is_touch_sensor = true;
            touched_pad = i;
            break;
        }
    }
    
    // Handle touch sensors (rising edge)
    if (is_touch_sensor) {
        // Only process rising edge for touch sensors
        if (events & GPIO_IRQ_EDGE_RISE) {
            printf("Touch sensor activated: GPIO %d\n", gpio);
            
            // Touch sensor was activated
            buttons_pressed[touched_pad] = true;
            
            // If in sound selection mode, configure the button
            if (sound_select_mode) {
                if (current_button_to_configure == touched_pad) {
                    // Button already selected, cycle through available sounds, make sure to wrap around
                    currently_selected_sound = (currently_selected_sound + 1) % total_num_tracks;

                    // switch the track that is currently assigned to the pad
                    button_sound_mapping[touched_pad] = currently_selected_sound;
                    
                    // update how long the samples are so that the sound playing works
                    total_samples[touched_pad] = available_sounds_sizes[currently_selected_sound];

                    // change the current list of tracks that we play
                    tracks[touched_pad] = available_sounds[currently_selected_sound];
                    
                    // Play the new sound so we can hear what we are selecting 
                    bit_set(tracks_playing, touched_pad);
                    samples_left_to_play[touched_pad] = total_samples[touched_pad];

                } else {
                    // just touched a new pad
                    // set it as the drum pad we are currently configuring
                    current_button_to_configure = touched_pad;

                    // update the array that holds the sound currently corresponding to each touch pad
                    currently_selected_sound = button_sound_mapping[touched_pad];   
                    
                    // Play the current sound before we make any changes
                    bit_set(tracks_playing, touched_pad);
                    samples_left_to_play[touched_pad] = total_samples[touched_pad];
                }
                return;
            }
            
            // SET THE BIT of the sound we want to play, the rest will be handled
            bit_set(tracks_playing, touched_pad);
            samples_left_to_play[touched_pad] = total_samples[touched_pad];
            
            if (record_mode) {
                add_loop_event(touched_pad);
            }
        }
        return;
    }
    
    // Handle push buttons (falling edge)
    // Only process falling edge for control buttons
    if (!(events & GPIO_IRQ_EDGE_FALL)) {
        return;
    }
    
    // Sound selection mode button
    if (gpio == SOUND_SELECT_PIN) {
        if (!gpio_get(SOUND_SELECT_PIN)) {  // Button pressed (active low)

            // every time we press the sound select pin just toggle the bool
            sound_select_mode = !sound_select_mode;
            
            // if we are in the sound select mode
            if (sound_select_mode) {

                current_button_to_configure = 0xFF; // No button selected yet
                currently_selected_sound = 0;
                
                // Turn off all the other modes so we don't get annoying overlap things
                record_mode = false;
                play_mode = false;
                classic_beat_mode = false;
                
                // Turn off mode LEDs
                gpio_put(RECORD_LED, 0);
                gpio_put(PLAY_LED, 0);
            } else {
                printf("SOUND SELECTION mode OFF\n");
            }
        }
        return;
    }
    
    // Record button
    if (gpio == RECORD_PIN) {
        if (!gpio_get(RECORD_PIN)) {  // if record wa spressed

            // ONLY go into this loop if we aren't picking a sound for one of the pads
            if (!sound_select_mode) {
                if (!record_mode) {
                    // if we aren't in record mode then start recording
                    record_mode = true;
                    classic_beat_mode = false; // stop playing premade beats

                    // so now start keeping track of time, 
                    loop_start_time = time_us_64();
                    loop_event_count = 0; // set the length to zero, we don't consider any of the previous loop as important now
                    gpio_put(RECORD_LED, 1); // Turn on record LED
                } 
                else {
                    // if we are in record mode then stop recording when we press the button
                    record_mode = false;
                    // now 
                    loop_end_time = time_us_64();
                    loop_duration = loop_end_time - loop_start_time;
                    printf("RECORD mode OFF - duration: %llu ms\n", loop_duration / 1000);

                    gpio_put(RECORD_LED, 0); // Turn off record LED
                    
                    // Automatically start playback if we have recorded any beats
                    if (loop_event_count > 0) {
                        play_mode = true;
                        loop_timestamp = 0;
                        gpio_put(PLAY_LED, 1); // Turn on play LED
                    }
                }
            }
        }
        return;
    }
    
    // Play button
    if (gpio == PLAY_PIN) {
        if (!gpio_get(PLAY_PIN)) {  // Button pressed (active low)
            printf("Play button pressed\n");
            // Only respond if not in sound selection mode
            if (!sound_select_mode) {
                play_mode = !play_mode;
                if (play_mode) {
                    printf("PLAY mode ON\n");
                    // Reset loop position when starting playback
                    loop_timestamp = 0;
                    gpio_put(PLAY_LED, 1); // Turn on play LED
                } else {
                    printf("PLAY mode OFF\n");
                    gpio_put(PLAY_LED, 0); // Turn off play LED
                }
            }
        }
        return;
    }
    
    // Clear button
    if (gpio == CLEAR_PIN) {
        if (!gpio_get(CLEAR_PIN)) {  // Button pressed (active low)
            printf("Clear button pressed\n");
            // Only respond if not in sound selection mode
            if (!sound_select_mode) {
                clear_loop();
                classic_beat_mode = false;
            }
        }
        return;
    }
    
    // Beat select button
    if (gpio == BEAT_SELECT_PIN) {
        if (!gpio_get(BEAT_SELECT_PIN)) {  // Button pressed (active low)
            printf("Beat select button pressed\n");
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
                gpio_put(RECORD_LED, 0);
                gpio_put(PLAY_LED, 1);                
            }
        }
        return;
    }
}

void check_loop_events() {

    // if we are in the playback loop mode
    // and if we have something in the loop
    if (play_mode && loop_duration > 0) {

        // loop through all of the loop events
        for (int i = 0; i < loop_event_count; i++) {

            // check if each event should have played yet,
            // have a window just to make sure we dont miss any of the beats, (even if they are slightly incorrectly timed)
            if (loop_events[i].timestamp <= loop_timestamp && 
                loop_events[i].timestamp > loop_timestamp - 5000) { // 5ms window

                uint8_t track = loop_events[i].track; // get the track that we should be playing

                if (track < num_active_tracks) { // if its an actual track
                    bit_set(tracks_playing, track);
                    samples_left_to_play[track] = total_samples[track];
                }
            }
        }
        
        // if we have completed the loop then set the timestamp back to zero so we start again
        if (loop_timestamp >= loop_duration) {

            loop_timestamp = 0; 
            
        }
    }
}

void update_leds() {

    // if we are recording, set the red LED on
    if (record_mode) {
        gpio_put(RECORD_LED, 1);
    } else {
        gpio_put(RECORD_LED, 0); 
    }


    // if we are in playback mode, then set the green LED on
    if (play_mode) {
        gpio_put(PLAY_LED, 1); 
    } else {
        gpio_put(PLAY_LED, 0); 
    }
}

// this will handle the timing for our loopoing
bool loop_timer_callback(struct repeating_timer *t) {
    
    // check the arduino in case theres an actual beat selected
    check_beat_selection_pins();
    
    // Update loop timestamp
    if (play_mode && loop_duration > 0) {
        loop_timestamp += 1000; // Increment by 1ms (timer is set to 1ms)
        check_loop_events();
    }
    
    // Updata our LEDs
    update_leds();
    
    return true;
}

// Timer callback function for audio sample generation
bool sample_timer_callback(struct repeating_timer *t) {
    int32_t mix = 0;
    int32_t samp_sum = 0;
    uint16_t pwm_val = 0;
    
    // Process active tracks and mix samples
    for (int i = 0; i < num_active_tracks; i++) { // for every track we have

        if (testbit(tracks_playing, i) == 1) { // if it is currently playing

            uint32_t current_sample_index = total_samples[i] - samples_left_to_play[i]; // get the sample we need to play

            if (current_sample_index < total_samples[i] && tracks[i] != NULL) {  // if we still have samples left to play
                samp_sum += tracks[i][current_sample_index];
                samples_left_to_play[i]--;  // Decrement number of samples left to play
                
                if (samples_left_to_play[i] <= 0) {
                    bit_clr(tracks_playing, i);  // Finished playing this track
                }
            } else {
                // we have completed playing that sample so just exit now

                bit_clr(tracks_playing, i); // clear the bit that set the sample to play
            
            }
        }
    }   

    // dont let the total mixed value go over 16 bits
    // just set it to the max if it goes over
    if (samp_sum < -32768) {
        samp_sum = -32768;
    } 
    else if (samp_sum > 32767) {
        samp_sum = 32767;
    }
    
    // Map from [-32768, 32767] to [0, 4096] 
    int32_t shifted_mix = (int32_t)mix + 32768;
    int32_t scaled_mix = shifted_mix * PWM_WRAP;
    uint16_t pwm_val = (uint16_t)(scaled_mix / 65536);
    
    pwm_set_gpio_level(pwm_output_pin, pwm_val);
    
    return true;
}

// Function to initialize pushbuttons with interrupts on falling edge
void init_pushbutton(uint pin) {
    // Configure pin as input with pull-up resistor
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    
    // Enable interrupt for falling edge (button press) and set callback
    gpio_set_irq_enabled_with_callback(pin, 
                                     GPIO_IRQ_EDGE_FALL, 
                                     true, 
                                     &gpio_isr);
}

// Function to initialize beat pins as simple inputs
void init_beat_pin(uint pin) {
    // Configure pin as input with pull-up resistor
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    
    // No interrupts needed for these pins
}

int main() {

    // get the pico sdk up and running
    stdio_init_all();
    
    sleep_ms(2000); // just wait a sec to make sure everything is chilling
    
    pwm_audio_init(); // get our PWM ready
    
    // Set up the touch pads for interupts
    for (int i = 0; i < num_active_tracks; i++) {
        gpio_init(Drum_Pads[i]);
        gpio_set_dir(Drum_Pads[i], GPIO_IN);
        
        // Use with_callback for all touch sensors
        gpio_set_irq_enabled_with_callback(Drum_Pads[i],        // cycle through all the pads
                                          GPIO_IRQ_EDGE_RISE,   // set at rising edge interupt
                                          true,                 // we want the interupt to be working
                                          &gpio_isr);           // use the gpio_isr function as a callback
    }
    
    const int total_pushbuttons = sizeof(pusshbuttons) / sizeof(pusshbuttons[0]);
    const int total_beat_pins = sizeof(beat_pins) / sizeof(beat_pins[0]);

    // Initialise the beat pins from arduino and also the mode pushbuttons
    for (int i = 0; i < total_pushbuttons; i++) {
        init_pushbutton(pusshbuttons[i]);
    }
        
    for (int i = 0; i < total_beat_pins; i++) {
        init_beat_pin(beat_pins[i]);
    }

    // Initialize LED indicator pins
    gpio_init(RECORD_LED);
    gpio_set_dir(RECORD_LED, GPIO_OUT);
    gpio_put(RECORD_LED, 0);  // Start with LED off
    
    gpio_init(PLAY_LED);
    gpio_set_dir(PLAY_LED, GPIO_OUT);
    gpio_put(PLAY_LED, 0);  // Start with LED off
    
    // Configure repeating timer for audio sample generation
    struct repeating_timer sample_timer;
    add_repeating_timer_us(-1000000 / SAMPLE_RATE, sample_timer_callback, NULL, &sample_timer);
    
    // Configure repeating timer for loop timing with 1ms precision
    struct repeating_timer loop_timer;
    add_repeating_timer_ms(-1, loop_timer_callback, NULL, &loop_timer);
    
    return 0;
}