#include <SPI.h>

// sinewave lookup
 int sine_table[] = {2048, 2098, 2148, 2199, 2249, 2299, 2349, 2399, 2448, 2498, 2547, 2596, 2644, 2692, 2740,
  2787, 2834, 2880, 2926, 2971, 3016, 3060, 3104, 3147, 3189, 3230, 3271, 3311, 3351, 3389, 
  3427, 3464, 3500, 3535, 3569, 3602, 3635, 3666, 3697, 3726, 3754, 3782, 3808, 3833, 3857, 
  3880, 3902, 3923, 3943, 3961, 3979, 3995, 4010, 4024, 4036, 4048, 4058, 4067, 4074, 4081, 
  4086, 4090, 4093, 4095, 4095, 4094, 4092, 4088, 4084, 4078, 4071, 4062, 4053, 4042, 4030, 
  4017, 4002, 3987, 3970, 3952, 3933, 3913, 3891, 3869, 3845, 3821, 3795, 3768, 3740, 3711, 
  3681, 3651, 3619, 3586, 3552, 3517, 3482, 3445, 3408, 3370, 3331, 3291, 3251, 3210, 3168, 
  3125, 3082, 3038, 2994, 2949, 2903, 2857, 2811, 2764, 2716, 2668, 2620, 2571, 2522, 2473, 
  2424, 2374, 2324, 2274, 2224, 2174, 2123, 2073, 2022, 1972, 1921, 1871, 1821, 1771, 1721, 
  1671, 1622, 1573, 1524, 1475, 1427, 1379, 1331, 1284, 1238, 1192, 1146, 1101, 1057, 1013, 
  970, 927, 885, 844, 804, 764, 725, 687, 650, 613, 578, 543, 509, 476, 444, 414, 384, 355, 
  327, 300, 274, 250, 226, 204, 182, 162, 143, 125, 108, 93, 78, 65, 53, 42, 33, 24, 17, 11, 
  7, 3, 1, 0, 0, 2, 5, 9, 14, 21, 28, 37, 47, 59, 71, 85, 100, 116, 134, 152, 172, 193, 215, 
  238, 262, 287, 313, 341, 369, 398, 429, 460, 493, 526, 560, 595, 631, 668, 706, 744, 784, 
  824, 865, 906, 948, 991, 1035, 1079, 1124, 1169, 1215, 1261, 1308, 1355, 1403, 1451, 1499, 
  1548, 1597, 1647, 1696, 1746, 1796, 1846, 1896, 1947, 1997, 2047};
  
  // sawtooth lookup table
  int sawtooth_table[] = {0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256, 272, 288, 304, 
  320, 336, 352, 368, 384, 400, 416, 432, 448, 464, 480, 496, 512, 528, 544, 560, 576, 592, 608, 
  624, 640, 656, 672, 688, 704, 720, 736, 752, 768, 784, 800, 816, 832, 848, 864, 880, 896, 912, 
  928, 944, 960, 976, 992, 1008, 1024, 1040, 1056, 1072, 1088, 1104, 1120, 1136, 1152, 1168, 1184, 
  1200, 1216, 1232, 1248, 1264, 1280, 1296, 1312, 1328, 1344, 1360, 1376, 1392, 1408, 1424, 1440, 
  1456, 1472, 1488, 1504, 1520, 1536, 1552, 1568, 1584, 1600, 1616, 1632, 1648, 1664, 1680, 1696, 
  1712, 1728, 1744, 1760, 1776, 1792, 1808, 1824, 1840, 1856, 1872, 1888, 1904, 1920, 1936, 1952, 
  1968, 1984, 2000, 2016, 2032, 2048, 2063, 2079, 2095, 2111, 2127, 2143, 2159, 2175, 2191, 2207, 
  2223, 2239, 2255, 2271, 2287, 2303, 2319, 2335, 2351, 2367, 2383, 2399, 2415, 2431, 2447, 2463, 
  2479, 2495, 2511, 2527, 2543, 2559, 2575, 2591, 2607, 2623, 2639, 2655, 2671, 2687, 2703, 2719, 
  2735, 2751, 2767, 2783, 2799, 2815, 2831, 2847, 2863, 2879, 2895, 2911, 2927, 2943, 2959, 2975, 
  2991, 3007, 3023, 3039, 3055, 3071, 3087, 3103, 3119, 3135, 3151, 3167, 3183, 3199, 3215, 3231, 
  3247, 3263, 3279, 3295, 3311, 3327, 3343, 3359, 3375, 3391, 3407, 3423, 3439, 3455, 3471, 3487, 
  3503, 3519, 3535, 3551, 3567, 3583, 3599, 3615, 3631, 3647, 3663, 3679, 3695, 3711, 3727, 3743, 
  3759, 3775, 3791, 3807, 3823, 3839, 3855, 3871, 3887, 3903, 3919, 3935, 3951, 3967, 3983, 3999, 
  4015, 4031, 4047, 4063, 4079};
  
  // square wave generator
  int square_table[] = {4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  // noise generator
  int noise_table[] = {2781, 1620, 1505, 4046, 154, 3625, 3740, 3261, 404, 1072, 1373, 2784, 559, 2954, 437, 2677, 2024, 
  3190, 2928, 3701, 3649, 1368, 2862, 810, 125, 3047, 2048, 1965, 3705, 2498, 2529, 3520, 3299, 2362, 
  749, 982, 3631, 117, 2006, 687, 4008, 2919, 2049, 1929, 244, 2793, 173, 292, 2136, 396, 3351, 3348, 
  2959, 613, 2701, 2124, 3985, 2658, 3278, 1858, 1771, 3380, 341, 545, 710, 1601, 3405, 3290, 247, 1635, 
  2158, 1707, 2690, 2572, 1195, 1768, 63, 4030, 684, 435, 1525, 811, 2005, 1390, 3897, 3769, 215, 3022, 
  1102, 1731, 2244, 3861, 1711, 4026, 1234, 2871, 2729, 2208, 2859, 2730, 729, 524, 4092, 700, 133, 2298, 
  3612, 2740, 780, 1511, 1887, 4020, 640, 3504, 2640, 1541, 782, 1754, 1974, 494, 2414, 926, 1575, 2387, 
  1031, 1189, 2527, 1086, 3376, 4024, 2991, 1408, 2392, 441, 3712, 3603, 3349, 1067, 2434, 92, 1741, 1280, 
  661, 732, 1732, 385, 2451, 1928, 2850, 2866, 2615, 137, 281, 1309, 2174, 2680, 1669, 3358, 2942, 3967, 
  2176, 1331, 432, 2502, 3189, 1734, 372, 1091, 629, 1150, 1802, 2159, 1873, 3585, 2121, 3865, 2612, 3922, 
  985, 2769, 1184, 2751, 2847, 278, 1043, 917, 2735, 3458, 1410, 3197, 2766, 27, 2466, 1584, 3751, 4, 1894, 
  1738, 1887, 3154, 1320, 3214, 1930, 146, 720, 2956, 1939, 625, 1397, 2487, 785, 3024, 994, 3757, 1102, 3135, 
  772, 1177, 373, 2360, 2799, 2238, 1743, 2639, 2652, 2781, 2604, 3871, 855, 2905, 967, 489, 2487, 1843, 1878, 
  2711, 3155, 1434, 2711, 1704, 3448, 3411, 1050, 2512, 2384, 2214, 3563, 1084, 1302, 488, 3849, 2644, 1963, 
  2618, 2231, 2651};
  

// Define pin assignments
const int CS_PIN = 10;       // Chip Select for the MCP4922 DAC

// Define array sizes
const int TABLE_SIZE = 256;

// Variables
volatile unsigned int counter = 0;
int waveformType = 0;        // 0: Sine, 1: Sawtooth, 2: Square, 3: Noise

// *** FREQUENCY SETTING ***
float frequency = 400;     // Set your desired frequency here in Hz (100-1000Hz range works well)

// Timing variables
unsigned long previousMicros = 0;
float phaseIncrement = 1.0;  // Will be calculated based on frequency
bool serialOutputEnabled = false; // Enable/disable serial output for debugging

void setup() {
  // Initialize Serial for debugging/plotting
  Serial.begin(115200);
  
  // Set up the Chip Select pin for the DAC
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);  // Ensure DAC is not selected initially
  
  // Initialize SPI communication
  SPI.begin();
  
  // Calculate phase increment based on frequency
  updatePhaseIncrement();
  
  Serial.println("Audio Signal Generator");
  Serial.println("---------------------");
  Serial.print("Frequency: ");
  Serial.print(frequency);
  Serial.println(" Hz");
  
  // Print the waveform type
  Serial.print("Waveform: ");
  switch (waveformType) {
    case 0: Serial.println("Sine"); break;
    case 1: Serial.println("Sawtooth"); break;
    case 2: Serial.println("Square"); break;
    case 3: Serial.println("Noise"); break;
  }
}

void loop() {
  // Time-critical section for generating audio
  unsigned long currentMicros = micros();
  
  // No delay - run as fast as possible with timing control
  if (currentMicros - previousMicros >= 1) { // Check every microsecond
    previousMicros = currentMicros;
    
    // Increment counter with phase increment for frequency control
    counter = (counter + (unsigned int)phaseIncrement) % TABLE_SIZE;
    
    // Get value from appropriate table
    int dacValue;
    switch (waveformType) {
      case 0: // Sine wave
        dacValue = sine_table[counter];
        break;
      case 1: // Sawtooth
        dacValue = sawtooth_table[counter];
        break;
      case 2: // Square
        dacValue = square_table[counter];
        break;
      case 3: // Noise
        dacValue = noise_table[counter];
        break;
      default:
        dacValue = sine_table[counter];
    }
    
    // Output to DAC
    writeToDAC(dacValue);
    
    // Optional serial output (significantly slows down the loop!)
    if (serialOutputEnabled && counter % 25 == 0) { // Only output every 25th sample
      Serial.println(dacValue);
    }
  }
}

void writeToDAC(int value) {
  // Build the 16-bit command word for the MCP4922 on channel A.
  // Command structure:
  // Bit 15: 0 (Channel A)
  // Bit 14: 1 (Buffered output enabled)
  // Bit 13: 1 (Gain = 1x)
  // Bit 12: 1 (Active mode)
  // Bits 11-0: 12-bit DAC data
  uint16_t command = 0x3000 | (value & 0x0FFF);

  // Begin an SPI transaction
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0)); // Fast SPI speed

  // Select the DAC by pulling CS low
  digitalWrite(CS_PIN, LOW);

  // Send the 16-bit command (split into two 8-bit transfers)
  SPI.transfer(highByte(command));
  SPI.transfer(lowByte(command));

  // Deselect the DAC
  digitalWrite(CS_PIN, HIGH);

  // End the SPI transaction
  SPI.endTransaction();
}

void updatePhaseIncrement() {
  // Constants for timing calculation
  const unsigned long loopTimeEstimate = 10; // Estimated microseconds per loop iteration
  
  // Calculate phase increment based on desired frequency
  // For a waveform of 256 points, we need to cycle through the entire table 
  // at the target frequency rate
  phaseIncrement = (frequency * TABLE_SIZE) / (1000000.0 / loopTimeEstimate);
  
  // Safety checks
  if (phaseIncrement < 0.1) phaseIncrement = 0.1;
  if (phaseIncrement > 127) phaseIncrement = 127; // Avoid overflows
}
