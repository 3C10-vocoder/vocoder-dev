//we can create a kick drum wooohoooo


#define MOZZI_CONTROL_RATE 64    // Hz (default, but shown for clarity)
#include <Mozzi.h>
#include <Oscil.h> 
#include <tables/sin2048_int8.h> 
#include <tables/triangle2048_int8.h>
#include <tables/whitenoise8192_int8.h>

// May have to split these up into different arduinos
// or a PICO with multiple threads
Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> kickSound(SIN2048_DATA);

Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> snareNoise(WHITENOISE8192_DATA);  
Oscil<TRIANGLE2048_NUM_CELLS, MOZZI_AUDIO_RATE> snareSound(TRIANGLE2048_DATA);  

Oscil<TRIANGLE2048_NUM_CELLS, MOZZI_AUDIO_RATE> tomSound(SIN2048_DATA);
// tom oscillator should maybe be a triangle wave??/


//      NOTE        //
//
//      atm this will only play kick drum on pin 3
//      probabily wont work
//      
//      snare and tom are included but may need their own arduino    
//
//


const float kick_startFreq = 150.0;  // Start frequency in Hz (punchy start)
const float kick_endFreq = 45.0;     // End frequency in Hz (deep thump)
const float kick_freqDecay = 0.90;   // Controls pitch drop speed
const float kick_ampDecay = 0.95;    // Controls volume fade speed
// for kick we would like a delay time of about  100 - 150 ms???

const float snare_startFreq = 220.0;  // Mid-frequency attack
const float snare_endFreq = 130.0;    // Resonant sustain
const float snare_freqDecay = 0.85;   // Faster drop-off for a snappy sound
const float snare_ampDecay = 0.92;    // Quick fade but not instant

const float tom_startFreq = 200.0;    // Mid tom attack frequency
const float tom_endFreq = 100.0;      // Resonant sustain
const float tom_freqDecay = 0.88;     // Smooth pitch drop
const float tom_ampDecay = 0.94;      // Slightly longer sustain

float kick_frequency;
float kick_amplitude;

float snare_frequency;
float snare_amplitude;

float tom_frequency;
float tom_amplitude;

bool KickisPlaying = false;      // True when we have kick drum sound playing
bool SnareisPlaying = false;     // True when we have snare drum sound playing
bool TomisPlaying = false;       // True when we have tom drum sound playing

const int kick = 3;
const int snare = 2;
const int tom = 999999; // can only have 2 easily set up interupts on pin 2 and 3, 
// will need to use another arduino or PICO or pin manipulation to get 

void setup() {
  startMozzi();

  // set interupt for the kick
  pinMode(kick, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(kick), triggerKick, RISING);
  
  // set interupt for the snare
  pinMode(snare, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(snare), triggerSnare, RISING);

}

void updateControl() { 

  if (KickisPlaying) { 
    // Exponentially drop frequency
    kick_frequency *= kick_freqDecay;           // decay the frequency and amplitude each call                  
    if (kick_frequency < kick_endFreq) {        // if we have done below the end frequency
      kick_frequency = kick_endFreq;                 // set frequency to the end frequency
      KickisPlaying = false;
    }
    kickSound.setFreq(kick_frequency);

    // Exponentially decay amplitude
    kick_amplitude *= kick_ampDecay;
    
  }

  if (SnareisPlaying) {
    // Exponentially drop frequency
    snare_frequency *= snare_freqDecay;
    if (snare_frequency < snare_endFreq) {
      snare_frequency = snare_endFreq;
    }
    snareNoise.setFreq(1000);
    snareSound.setFreq(snare_frequency);
    // Exponentially decay amplitude
    snare_amplitude *= snare_ampDecay;
    
  }

  if (TomisPlaying) {
    // Exponentially drop frequency
    tom_frequency *= tom_freqDecay;
    if (tom_frequency < tom_endFreq) {
      tom_frequency = tom_endFreq;
    }
    tomSound.setFreq(tom_frequency);

    // Exponentially decay amplitude
    tom_amplitude *= tom_ampDecay;
    
  }
}

AudioOutput updateAudio() { // ISSUE

  // i dont know how to have the different waves/drums at different amplitude while:
  //    a. maintaining the total volume, rather than just mixing relative amplitudes
  //    b. i can only see to do this by using 3 arduinos, each generating their own drum 
  //        this is maybe possible???


  if (KickisPlaying || SnareisPlaying || TomisPlaying) {
    return MonoOutput::from8Bit((kickSound.next() * kick_amplitude) / 255); // Apply volume decay

  } else {
    return MonoOutput::from8Bit(0);  // Silence when not playing
  }
}

void loop() {
  audioHook();
}

// Function to trigger the kick drum
void triggerKick() {
  KickisPlaying = true;
  kick_frequency = kick_startFreq;
  kick_amplitude = 255;
}

//function to trigger the snare drum
void triggerSnare() {
  SnareisPlaying = true;
  snare_frequency = snare_startFreq;
  snare_amplitude = 255;
}

// function to trigger the tom drum
void triggerTom() {
  TomisPlaying = true;
  tom_frequency = tom_startFreq;
  tom_amplitude = 255;
}