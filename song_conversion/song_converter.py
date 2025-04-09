import numpy as np
from scipy.io import wavfile

def load_audio(file_path):
    """load audio file and return sample rate and data"""
    sr, data = wavfile.read(file_path)

    # if wav is not in float form
    if data.dtype != np.float64:
        # if it is an integer
        if np.issubdtype(data.dtype, np.integer):
            # get info about the data
            info = np.iinfo(data.dtype)
            # convert to an array of float64 type normalized to 1
            data = data.astype(np.float64) / max(abs(info.min), abs(info.max))
        else:
            data = data.astype(np.float64)
    # check for multi-channel
    if len(data.shape) > 1:
        data = np.mean(data, axis=1)
    return sr, data

def normalize_audio_int16(data):
    """normalize data to int16 range"""
    # get the maximum data value to check if it has been correctly normalized
    max_val = np.max(np.abs(data))
    # if max_val > 1.0, normalize, otherwise data is assumed normalized
    if max_val > 1.0:
        norm = data / max_val  
    else:
        norm = data
    # now scale the data to the int16_t datatype which is required for PWM output
    scaled = norm * 32767.0
    return np.round(scaled).astype(np.int16)

def resample_audio(data, orig_rate, target_rate):
    """resample audio using linear interpolation"""
    ratio = target_rate / orig_rate
    # new vector length will be smaller than original
    new_len = int(np.round(len(data) * ratio))
    # generate the inices for resampling -> evenly spaced between 0 - len(data) - 1
    indices = np.linspace(0, len(data) - 1, new_len)
    orig_indices = np.arange(len(data))
    # extract data from old indices of data at new indices
    return np.interp(indices, orig_indices, data)

def create_header_file(data, sr, out_file, array_name="audio_data"):
    """create c header file with int16_t array of audio data"""
    dur = len(data) / sr

    with open(out_file, 'w') as f:
        f.write(f"// audio data for: {array_name}\n")
        f.write(f"// sample rate: {sr} hz\n")
        f.write(f"// duration: {dur:.3f} seconds\n")
        f.write(f"// length: {len(data)} samples\n\n")
        f.write("#ifndef AUDIO_DATA_H_%s\n" % array_name.upper())
        f.write("#define AUDIO_DATA_H_%s\n\n" % array_name.upper())
        f.write("#include <stdint.h>\n")
        f.write("#include <stddef.h>\n\n")
        f.write(f"const size_t {array_name}_length = {len(data)};\n\n")
        f.write(f"const int16_t {array_name}[{len(data)}] = {{\n    ")

        vals_per_line = 12
        for i, v in enumerate(data):
            f.write(f"{v}")
            if i < len(data) - 1:
                f.write(",")
                if (i + 1) % vals_per_line == 0:
                    f.write("\n    ")
                else:
                    f.write (" ")
            else:
                f.write("\n")

        f.write("};\n\n")
        f.write("#endif // AUDIO_DATA_H_%s\n" % array_name.upper())

if __name__ == "__main__":
    wav_file = "blk_enter_dragon.wav"
    header_file = "blk_enter_dragon.h"
    array_name = "blk_enter_dragon_audio"
    target_sr = 22050
    max_duration = 4.0  # seconds

    orig_sr, audio = load_audio(wav_file)
    # shorten audio array to 4 seconds
    audio = audio[:int(max_duration * orig_sr)]

    # sample the new audio file at that bit width
    if target_sr != orig_sr:
        audio = resample_audio(audio, orig_sr, target_sr)
        orig_sr = target_sr

    audio_int16 = normalize_audio_int16(audio)
    create_header_file(audio_int16, orig_sr, header_file, array_name)
