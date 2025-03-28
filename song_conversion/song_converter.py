import numpy as np
from scipy.io import wavfile
import matplotlib.pyplot as plt

def load_audio(file_path):
    """
    Load an audio file and return the sample rate and data.
    """
    sample_rate, audio_data = wavfile.read(file_path)
    
    # If stereo, convert to mono by averaging channels
    if len(audio_data.shape) > 1:
        audio_data = np.mean(audio_data, axis=1)
    
    return sample_rate, audio_data

def normalize_audio(audio_data, bit_depth=16):
    """
    Normalize audio data to the range appropriate for the given bit depth.
    For DAC output, typically we want values between 0 and 2^bit_depth-1.
    """
    # Scale to 0-1 range
    audio_normalized = (audio_data - np.min(audio_data)) / (np.max(audio_data) - np.min(audio_data))
    
    # Scale to appropriate range for bit depth
    max_value = 2**bit_depth - 1
    audio_scaled = np.round(audio_normalized * max_value).astype(np.uint16)
    
    return audio_scaled

def resample_audio(audio_data, original_rate, target_rate):
    """
    Resample audio data to a different sampling rate.
    """
    # Calculate ratio of target rate to original rate
    ratio = target_rate / original_rate
    
    # Calculate number of samples for the new rate
    new_length = int(len(audio_data) * ratio)
    
    # Create indices for the new samples
    indices = np.linspace(0, len(audio_data) - 1, new_length)
    
    # Use linear interpolation to resample
    resampled_audio = np.interp(indices, np.arange(len(audio_data)), audio_data)
    
    return resampled_audio

def save_to_file(audio_data, output_file, format_type="decimal"):
    """
    Save the processed audio data to a file in the specified format.
    
    format_type options:
    - "decimal": Plain decimal values
    - "hex": Hexadecimal values
    - "binary": Binary values
    - "csv": Comma-separated decimal values
    """
    with open(output_file, 'w') as f:
        if format_type == "decimal":
            for value in audio_data:
                f.write(f"{value}\n")
        elif format_type == "hex":
            for value in audio_data:
                f.write(f"0x{value:04X}\n")
        elif format_type == "binary":
            for value in audio_data:
                f.write(f"{value:016b}\n")
        elif format_type == "csv":
            f.write(','.join(map(str, audio_data)))
        else:
            raise ValueError(f"Unsupported format type: {format_type}")

def plot_audio(audio_data, sample_rate, title="Audio Waveform"):
    """
    Plot the audio data for visualization.
    """
    duration = len(audio_data) / sample_rate
    time = np.linspace(0, duration, len(audio_data))
    
    plt.figure(figsize=(10, 6))
    plt.plot(time, audio_data)
    plt.title(title)
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.grid(True)
    plt.show()

def create_header_file(audio_data, output_file, array_name="audio_data"):
    """
    Create a C header file with the audio data as an array.
    Useful for embedding audio in microcontroller applications.
    """
    with open(output_file, 'w') as f:
        f.write(f"const unsigned short {array_name}[{len(audio_data)}] = {{\n    ")
        
        # Write 12 values per line
        values_per_line = 12
        for i in range(0, len(audio_data), values_per_line):
            line_values = audio_data[i:i+values_per_line]
            f.write(', '.join([f"{value}" for value in line_values]))
            
            if i + values_per_line < len(audio_data):
                f.write(",\n    ")
            else:
                f.write("\n};\n\n")
                
        f.write(f"const unsigned int {array_name}_length = {len(audio_data)};\n")
        f.write(f"// Original sample rate: {sample_rate} Hz\n")

if __name__ == "__main__":
    # Input parameters
    input_file = "kleber_piano_loop.wav"  # Path to your audio file
    output_file = "kleber_piano_loop.txt"  # Output file for raw values
    header_file = "audio_data.h"    # Output header file for C/C++
    target_sample_rate = 44100      # Target sampling rate in Hz
    bit_depth = 12                  # Bit depth for DAC (common values: 8, 10, 12, 16)
    
    # Load audio file
    sample_rate, audio_data = load_audio(input_file)
    print(f"Original sample rate: {sample_rate} Hz")
    print(f"Original audio length: {len(audio_data)} samples")
    
    # Resample if needed
    if target_sample_rate != sample_rate:
        audio_data = resample_audio(audio_data, sample_rate, target_sample_rate)
        sample_rate = target_sample_rate
        print(f"Resampled to: {sample_rate} Hz")
        print(f"New audio length: {len(audio_data)} samples")
    
    # Normalize and scale for DAC
    dac_values = normalize_audio(audio_data, bit_depth)
    
    # Plot the processed audio
    plot_audio(dac_values, sample_rate, title=f"Audio Waveform (Normalized to {bit_depth}-bit)")
    
    # Save the values to a text file
    save_to_file(dac_values, output_file, format_type="decimal")
    print(f"Saved {len(dac_values)} DAC values to {output_file}")
    
    # Create a C header file
    create_header_file(dac_values, header_file)
    print(f"Created C header file: {header_file}")
    
    print("\nSample output values:")
    print(dac_values[:10])  # Print first 10 values