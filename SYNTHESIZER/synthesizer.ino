#include <SPI.h>

// -------------------- Pin Definitions --------------------
#define CS_PIN           10
#define OCT_UP_PIN       A7
#define OCT_DOWN_PIN     A6
#define FREQ_TUNE_PIN    A0
#define SEMITONE_TUNE_PIN A1
// ADSR pots on A2–A5:
#define ATTACK_POT       A2    // Attack
#define DECAY_POT        A3    // Decay (used here to set decay rate only)
#define SUSTAIN_POT      A4    // Sustain (captured at note-on)
#define RELEASE_POT      A5    // Release

// Note buttons (pins D2–D9)
const uint8_t NOTE_PINS[] = {2, 3, 4, 5, 6, 7, 8, 9};
const uint8_t NUM_NOTES = sizeof(NOTE_PINS);

// ------------------- DAC & Frequency Scaling -------------------
#define DAC_CONFIG       0x3000  // For MCP4922 DAC: channel A, 1x gain, active
#define FREQUENCY_SCALING 8.4    // Empirically determined scaling factor

// ----------------------- Waveform Table -------------------------
// 256-point lookup table for a rich waveform (12-bit, centered at 2048)
const int rich_lookup_table[256] = {
    2048, 2207, 2364, 2520, 2672, 2820, 2963, 3101,
    3231, 3354, 3469, 3576, 3672, 3760, 3837, 3904,
    3961, 4008, 4044, 4071, 4088, 4095, 4094, 4084,
    4066, 4042, 4011, 3974, 3932, 3886, 3837, 3785,
    3732, 3677, 3623, 3568, 3515, 3464, 3415, 3368,
    3325, 3285, 3249, 3216, 3188, 3164, 3145, 3129,
    3117, 3109, 3104, 3102, 3103, 3107, 3112, 3119,
    3127, 3135, 3144, 3152, 3160, 3166, 3171, 3175,
    3176, 3175, 3171, 3164, 3155, 3143, 3128, 3110,
    3089, 3066, 3040, 3012, 2982, 2950, 2917, 2883,
    2848, 2813, 2778, 2743, 2709, 2676, 2644, 2614,
    2585, 2559, 2534, 2512, 2492, 2474, 2459, 2446,
    2436, 2427, 2421, 2416, 2413, 2411, 2410, 2410,
    2410, 2411, 2411, 2412, 2411, 2409, 2407, 2402,
    2396, 2389, 2379, 2367, 2353, 2337, 2319, 2299,
    2277, 2253, 2227, 2200, 2171, 2141, 2111, 2079,
    2048, 2017, 1985, 1955, 1925, 1896, 1869, 1843,
    1819, 1797, 1777, 1759, 1743, 1729, 1717, 1707,
    1700, 1694, 1689, 1687, 1685, 1684, 1685, 1685,
    1686, 1686, 1686, 1685, 1683, 1680, 1675, 1669,
    1660, 1650, 1637, 1622, 1604, 1584, 1562, 1537,
    1511, 1482, 1452, 1420, 1387, 1353, 1318, 1283,
    1248, 1213, 1179, 1146, 1114, 1084, 1056, 1030,
    1007, 986, 968, 953, 941, 932, 925, 921,
    920, 921, 925, 930, 936, 944, 952, 961,
    969, 977, 984, 989, 993, 994, 992, 987,
    979, 967, 951, 932, 908, 880, 847, 811,
    771, 728, 681, 632, 581, 528, 473, 419,
    364, 311, 259, 210, 164, 122, 85, 54,
    30, 12, 2, 1, 8, 25, 52, 88,
    135, 192, 259, 336, 424, 520, 627, 742,
    865, 995, 1133, 1276, 1424, 1576, 1732, 1889
};

// -------------------- Base Frequencies ------------------------
// For notes A4, B4, C5, D5, E5, F5, G5, A5 (using exact frequency ratios from A4=440Hz)
const float BASE_FREQUENCIES[] = {
    440.00, 493.88, 523.25, 587.33, 659.26, 698.46, 783.99, 880.00
};

// -------------------- ADSR Definitions -------------------------
// Envelope states:
#define ENV_IDLE    0
#define ENV_ATTACK  1
#define ENV_DECAY   2
#define ENV_SUSTAIN 3
#define ENV_RELEASE 4

// Global ADSR rates for attack, decay, and release (decay rate remains global)
float env_attack  = 0.005;   // Attack rate (increment per sample)
float env_decay   = 0.005;   // Decay rate (decrement per sample)
float env_release = 0.005;   // Release rate (decrement per sample)

// In order to decouple decay from sustain, we add per‑voice fields.
// (When a note is triggered the current DECAY and SUSTAIN pot values are captured.)
struct Voice {
    bool active;           // Is the voice active?
    float phase;           // Current phase (0–256, fractional)
    float phaseIncrement;  // Phase increment per sample (set by frequency)
    uint8_t noteIndex;     // Note index (0–7)
    float baseFreq;        // Frequency before scaling adjustments
    float envLevel;        // Current envelope level (0.0 to 1.0)
    uint8_t envState;      // ADSR state (IDLE, ATTACK, etc.)
    float decayTarget;     // Target level for decay (captured at note-on)
    float sustainLevel;    // Sustain level (captured at note-on)
};

Voice voices[2] = {
    {false, 0.0, 0.0, 0, 0.0, 0.0, ENV_IDLE, 0.0, 0.0},
    {false, 0.0, 0.0, 0, 0.0, 0.0, ENV_IDLE, 0.0, 0.0}
};
uint8_t currentVoice = 0;       // For round-robin selection
int8_t octaveOffset = 0;        // Octave shift (-3 to +4)
uint8_t lastButtonState[NUM_NOTES] = {0}; // For debouncing buttons
uint32_t lastOctaveChangeTime = 0;
uint32_t lastNoteChangeTime[NUM_NOTES] = {0};
float baseFreqMultiplier = 1.0; // Fine tuning multiplier
float semitoneRatio = 1.0595;   // Ratio for one semitone
uint32_t seedValue = 0;         // For phase randomization

// -------------------- Update Envelope -------------------------
// During DECAY the envelope decreases from 1.0 to the per‑voice decay target, then jumps to the captured sustain level.
inline void updateEnvelope(Voice &v) {
    if (v.envState == ENV_ATTACK) {
    v.envLevel += env_attack;
    if (v.envLevel >= 1.0) {
        v.envLevel = 1.0;
        v.envState = ENV_DECAY;
    }
    } else if (v.envState == ENV_DECAY) {
    v.envLevel -= env_decay;
    if (v.envLevel <= v.decayTarget) {    // Use per‑voice decay target
        v.envLevel = v.sustainLevel;        // Jump to the captured sustain level
        v.envState = ENV_SUSTAIN;
    }
    } else if (v.envState == ENV_SUSTAIN) {
    v.envLevel = v.sustainLevel;
    } else if (v.envState == ENV_RELEASE) {
    v.envLevel -= env_release;
    if (v.envLevel <= 0.0) {
        v.envLevel = 0.0;
        v.envState = ENV_IDLE;
        v.active = false;
    }
    }
}

// -------------------- DAC Output -------------------------
inline void writeToDAC(uint16_t value) {
    uint16_t command = DAC_CONFIG | (value & 0x0FFF);
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(command >> 8);
    SPI.transfer(command & 0xFF);
    digitalWrite(CS_PIN, HIGH);
}

// -------------------- Audio Generation -------------------------
// This function is called repeatedly from loop() to generate audio.
inline int generateAudio() {
    float mixedAC = 0.0;
    const float weight0 = 0.55;
    const float weight1 = 0.45;
    for (uint8_t i = 0; i < 2; i++) {
    if (!voices[i].active) continue;
    updateEnvelope(voices[i]);
    voices[i].phase += voices[i].phaseIncrement;
    while (voices[i].phase >= 256)
        voices[i].phase -= 256;
    uint8_t sampleIndex = (uint8_t)voices[i].phase;
    int sample = rich_lookup_table[sampleIndex];
    float acSample = (sample - 2048) * voices[i].envLevel;
    if (i == 0)
        mixedAC += weight0 * acSample;
    else
        mixedAC += weight1 * acSample;
    }
    return (int)(mixedAC + 2048);
}

// -------------------- Update Active Voice Frequencies -------------------------
void updateAllVoiceFrequencies() {
    float baseA4 = 440.0 * baseFreqMultiplier;
    float noteFrequencies[NUM_NOTES];
    int semitoneDistance[] = {0, 2, 3, 5, 7, 8, 10, 12};
    for (int i = 0; i < NUM_NOTES; i++) {
    noteFrequencies[i] = baseA4 * pow(semitoneRatio, semitoneDistance[i]);
    }
    for (uint8_t v = 0; v < 2; v++) {
    if (voices[v].active) {
        float frequency = noteFrequencies[voices[v].noteIndex];
        if (octaveOffset > 0) {
        for (int8_t j = 0; j < octaveOffset; j++)
            frequency *= 2.0;
        } else if (octaveOffset < 0) {
        for (int8_t j = 0; j > octaveOffset; j--)
            frequency *= 0.5;
        }
        voices[v].baseFreq = frequency;
        float scalingFactor = FREQUENCY_SCALING;
        if (frequency > 2000)
        scalingFactor *= 1.1;
        float newPhaseInc = (frequency * scalingFactor) / 1000.0;
        if (newPhaseInc > 128)
        newPhaseInc = 128;
        if (fabs(newPhaseInc - voices[v].phaseIncrement) > 0.1)
        voices[v].phaseIncrement = newPhaseInc;
    }
    }
}

// -------------------- Input Processing -------------------------
void checkInputs() {
    // --- Frequency and Tuning ---
    int freqTune = analogRead(FREQ_TUNE_PIN);
    baseFreqMultiplier = 0.6 + (0.8 * freqTune / 1023.0);
    
    int semitoneTune = analogRead(SEMITONE_TUNE_PIN);
    semitoneRatio = pow(2.0, (0.85 + (0.3 * semitoneTune / 1023.0)) / 12.0);
    
    // --- ADSR Controls for Attack & Release (global) ---
    int attackPot = analogRead(ATTACK_POT);
    int releasePot = analogRead(RELEASE_POT);
    env_attack  = 0.0001 + 0.0002 * (attackPot / 1023.0);
    env_release = 0.0001 + 0.0002 * (releasePot / 1023.0);
    
    // For decay and sustain, we capture per-note values at note-on.
    // Optionally update live sustain for voices in sustain:
    int sustainPot = analogRead(SUSTAIN_POT);
    float liveSustain = (float)sustainPot / 1023.0;
    for (uint8_t v = 0; v < 2; v++) {
    if (voices[v].active && voices[v].envState == ENV_SUSTAIN) {
        voices[v].sustainLevel = liveSustain;
        voices[v].envLevel = liveSustain;
    }
    }
    
    // --- Update Active Voice Frequencies ---
    if (voices[0].active || voices[1].active)
    updateAllVoiceFrequencies();
    
    // --- Octave Control ---
    uint32_t currentTime = millis();
    if (currentTime - lastOctaveChangeTime > 200) {
    if (digitalRead(OCT_UP_PIN) == LOW && octaveOffset < 4) {
        octaveOffset++;
        lastOctaveChangeTime = currentTime;
        updateAllVoiceFrequencies();
    } else if (digitalRead(OCT_DOWN_PIN) == LOW && octaveOffset > -3) {
        octaveOffset--;
        lastOctaveChangeTime = currentTime;
        updateAllVoiceFrequencies();
    }
    }
    
    // --- Note Button Handling (with debouncing) ---
    for (uint8_t i = 0; i < NUM_NOTES; i++) {
    uint8_t buttonState = digitalRead(NOTE_PINS[i]);
    if (buttonState != lastButtonState[i] && (currentTime - lastNoteChangeTime[i] > 20)) {
        lastNoteChangeTime[i] = currentTime;
        lastButtonState[i] = buttonState;
        if (buttonState == LOW)
        triggerNote(i);
        else
        releaseNote(i);
    }
    }
} // <-- End of checkInputs()

// -------------------- Note Triggering -------------------------
void triggerNote(uint8_t noteIndex) {
    // If the note is already active, reinitialize that voice.
    for (uint8_t v = 0; v < 2; v++) {
    if (voices[v].active && voices[v].noteIndex == noteIndex) {
        float baseA4 = 440.0 * baseFreqMultiplier;
        int semitoneDistance[] = {0, 2, 3, 5, 7, 8, 10, 12};
        float frequency = baseA4 * pow(semitoneRatio, semitoneDistance[noteIndex]);
        if (octaveOffset > 0) {
        for (int8_t j = 0; j < octaveOffset; j++)
            frequency *= 2.0;
        } else if (octaveOffset < 0) {
        for (int8_t j = 0; j > octaveOffset; j--)
            frequency *= 0.5;
        }
        voices[v].baseFreq = frequency;
        float scalingFactor = FREQUENCY_SCALING;
        if (frequency > 2000)
        scalingFactor *= 1.1;
        float phaseInc = (frequency * scalingFactor) / 1000.0;
        if (phaseInc > 128)
        phaseInc = 128;
        voices[v].phaseIncrement = phaseInc;
        voices[v].phase = (seedValue % 256);
        voices[v].envLevel = 0.0;
        voices[v].envState = ENV_ATTACK;
        // Capture decay and sustain settings at note-on
        voices[v].decayTarget = 0.2 + 0.7 * (analogRead(DECAY_POT) / 1023.0);
        voices[v].sustainLevel = (float)analogRead(SUSTAIN_POT) / 1023.0;
        return;
    }
    }
    
    // Otherwise, choose a free voice (or use round-robin if both are active)
    uint8_t voiceIndex = 0;
    if (voices[0].active && voices[1].active) {
    voiceIndex = currentVoice;
    currentVoice = 1 - currentVoice;
    } else if (voices[0].active) {
    voiceIndex = 1;
    } else {
    voiceIndex = 0;
    }
    
    float baseA4 = 440.0 * baseFreqMultiplier;
    int semitoneDistance[] = {0, 2, 3, 5, 7, 8, 10, 12};
    float frequency = baseA4 * pow(semitoneRatio, semitoneDistance[noteIndex]);
    if (octaveOffset > 0) {
    for (int8_t j = 0; j < octaveOffset; j++)
        frequency *= 2.0;
    } else if (octaveOffset < 0) {
    for (int8_t j = 0; j > octaveOffset; j--)
        frequency *= 0.5;
    }
    voices[voiceIndex].baseFreq = frequency;
    float scalingFactor = FREQUENCY_SCALING;
    if (frequency > 2000)
    scalingFactor *= 1.1;
    float phaseInc = (frequency * scalingFactor) / 1000.0;
    if (phaseInc > 128)
    phaseInc = 128;
    voices[voiceIndex].phase = (seedValue % 256);
    voices[voiceIndex].phaseIncrement = phaseInc;
    voices[voiceIndex].active = true;
    voices[voiceIndex].noteIndex = noteIndex;
    voices[voiceIndex].envLevel = 0.0;
    voices[voiceIndex].envState = ENV_ATTACK;
    // Capture independent decay and sustain values on note-on
    voices[voiceIndex].decayTarget = 0.2 + 0.7 * (analogRead(DECAY_POT) / 1023.0);
    voices[voiceIndex].sustainLevel = (float)analogRead(SUSTAIN_POT) / 1023.0;
}

// -------------------- Note Release -------------------------
void releaseNote(uint8_t noteIndex) {
    // Initiate the release phase for the voice playing this note.
    for (uint8_t v = 0; v < 2; v++) {
    if (voices[v].active && voices[v].noteIndex == noteIndex) {
        voices[v].envState = ENV_RELEASE;
        break;
    }
    }
}

// -------------------- DAC Initialization -------------------------
void setup() {
    // Set up DAC chip select
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    
    // Initialize SPI for DAC
    SPI.begin();
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    
    // Set up note buttons with pull-ups
    for (uint8_t i = 0; i < NUM_NOTES; i++) {
    pinMode(NOTE_PINS[i], INPUT_PULLUP);
    }
    // Set up octave control buttons
    pinMode(OCT_UP_PIN, INPUT_PULLUP);
    pinMode(OCT_DOWN_PIN, INPUT_PULLUP);
    
    // Initialize random seed (for phase randomization)
    seedValue = micros();
    
    // Start with a silent output
    writeToDAC(2048);
}

// -------------------- Main Loop -------------------------
void loop() {
    // Generate and output audio as fast as possible
    writeToDAC(generateAudio());
    
    // Check inputs about every 100ms
    static uint32_t lastPotRead = 0;
    if (millis() - lastPotRead > 100) {
    lastPotRead = millis();
    checkInputs();
    }
    
    // Update random seed occasionally
    seedValue = seedValue * 1664525L + 1013904223L;
}