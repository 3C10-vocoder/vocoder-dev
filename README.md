# mozzi template

# Mozzi library template

#define MOZZI_CONTROL_RATE 64    // Hz, powers of 2 are most reliable; 64 Hz is actually the default, but shown here, for clarity
#include <Mozzi.h>
#include <Oscil.h> // oscillator template
#include <tables/sin2048_int8.h> // sine table for oscillator

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> aSin(SIN2048_DATA);

void setup(){
  startMozzi(); // :)
  aSin.setFreq(440); // set the frequency
}


void updateControl(){
  // put changing controls in here
}


AudioOutput updateAudio(){
  return MonoOutput::from8Bit(aSin.next()); // return an int signal centred around 0
}


void loop(){
  audioHook(); // required here
}


