#include <SPI.h>

// Define pin assignments
const int analogPin = A0;    // Analog input for your audio signal
const int CS_PIN = 10;       // Chip Select for the MCP4922 DAC

void setup() {
  // Initialize Serial for debugging/plotting (Serial Plotter)
  Serial.begin(115200);
  // Wait for the Serial connection to be established (optional)
  while (!Serial) {
    ; // wait for serial port to connect.
  }

  // Set up the Chip Select pin for the DAC
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);  // Ensure DAC is not selected initially
  
  // Initialize SPI communication
  SPI.begin();
}

void loop() {
  // Read the analog signal (0 - 1023, corresponding to 0 - 3.3V)
  int adcValue = analogRead(analogPin);

  // Map the 10-bit ADC value to a 12-bit range (0 - 4095)
  int dacValue = map(adcValue, 0, 1023, 0, 4095);

  // Build the 16-bit command word for the MCP4922 on channel A.
  // Command structure:
  // Bit 15: 0 (Channel A)
  // Bit 14: 1 (Buffered output enabled)
  // Bit 13: 1 (Gain = 1x)
  // Bit 12: 1 (Active mode)
  // Bits 11-0: 12-bit DAC data
  uint16_t command = 0x3000 | (dacValue & 0x0FFF);

  // Begin an SPI transaction
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  // Select the DAC by pulling CS low
  digitalWrite(CS_PIN, LOW);

  // Send the 16-bit command (split into two 8-bit transfers)
  SPI.transfer(highByte(command));
  SPI.transfer(lowByte(command));

  // Deselect the DAC
  digitalWrite(CS_PIN, HIGH);

  // End the SPI transaction
  SPI.endTransaction();

  // Output the ADC value to the Serial Plotter for real-time graphing
  Serial.println(adcValue);

  // Delay to control the sample rate (adjust as necessary)
  delay(1);  // ~1ms delay gives roughly a 1 kHz sample rate
}