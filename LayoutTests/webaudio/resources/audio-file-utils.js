// Utilities for creating a 16-bit PCM WAV file from an AudioBuffer
// when using Chrome testRunner, and for downloading an AudioBuffer as
// a float WAV file when running in a browser.

function writeString(s, a, offset) {
  for (let i = 0; i < s.length; ++i) {
    a[offset + i] = s.charCodeAt(i);
  }
}

function writeInt16(n, a, offset) {
  n = Math.floor(n);

  let b1 = n & 255;
  let b2 = (n >> 8) & 255;

  a[offset + 0] = b1;
  a[offset + 1] = b2;
}

function writeInt32(n, a, offset) {
  n = Math.floor(n);
  let b1 = n & 255;
  let b2 = (n >> 8) & 255;
  let b3 = (n >> 16) & 255;
  let b4 = (n >> 24) & 255;

  a[offset + 0] = b1;
  a[offset + 1] = b2;
  a[offset + 2] = b3;
  a[offset + 3] = b4;
}

// Return the bits of the float as a 32-bit integer value.  This
// produces the raw bits; no intepretation of the value is done.
function floatBits(f) {
  let buf = new ArrayBuffer(4);
  (new Float32Array(buf))[0] = f;
  let bits = (new Uint32Array(buf))[0];
  // Return as a signed integer.
  return bits | 0;
}

function writeAudioBuffer(audioBuffer, a, offset, asFloat) {
  let n = audioBuffer.length;
  let channels = audioBuffer.numberOfChannels;

  for (let i = 0; i < n; ++i) {
    for (let k = 0; k < channels; ++k) {
      let buffer = audioBuffer.getChannelData(k);
      if (asFloat) {
        let sample = floatBits(buffer[i]);
        writeInt32(sample, a, offset);
        offset += 4;
      } else {
        let sample = buffer[i] * 32768.0;

        // Clip samples to the limitations of 16-bit.
        // If we don't do this then we'll get nasty wrap-around distortion.
        if (sample < -32768)
          sample = -32768;
        if (sample > 32767)
          sample = 32767;

        writeInt16(sample, a, offset);
        offset += 2;
      }
    }
  }
}

// See http://soundfile.sapp.org/doc/WaveFormat/ and
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
// for a quick introduction to the WAVE PCM format.
function createWaveFileData(audioBuffer, asFloat) {
  let bytesPerSample = asFloat ? 4 : 2;
  let frameLength = audioBuffer.length;
  let numberOfChannels = audioBuffer.numberOfChannels;
  let sampleRate = audioBuffer.sampleRate;
  let bitsPerSample = 8 * bytesPerSample;
  let byteRate = sampleRate * numberOfChannels * bitsPerSample / 8;
  let blockAlign = numberOfChannels * bitsPerSample / 8;
  let wavDataByteLength = frameLength * numberOfChannels * bytesPerSample;
  let headerByteLength = 44;
  let totalLength = headerByteLength + wavDataByteLength;

  let waveFileData = new Uint8Array(totalLength);

  let subChunk1Size = 16;  // for linear PCM
  let subChunk2Size = wavDataByteLength;
  let chunkSize = 4 + (8 + subChunk1Size) + (8 + subChunk2Size);

  writeString('RIFF', waveFileData, 0);
  writeInt32(chunkSize, waveFileData, 4);
  writeString('WAVE', waveFileData, 8);
  writeString('fmt ', waveFileData, 12);

  writeInt32(subChunk1Size, waveFileData, 16);  // SubChunk1Size (4)
  // The format tag value is 1 for integer PCM data and 3 for IEEE
  // float data.
  writeInt16(asFloat ? 3 : 1, waveFileData, 20);   // AudioFormat (2)
  writeInt16(numberOfChannels, waveFileData, 22);  // NumChannels (2)
  writeInt32(sampleRate, waveFileData, 24);        // SampleRate (4)
  writeInt32(byteRate, waveFileData, 28);          // ByteRate (4)
  writeInt16(blockAlign, waveFileData, 32);        // BlockAlign (2)
  writeInt32(bitsPerSample, waveFileData, 34);     // BitsPerSample (4)

  writeString('data', waveFileData, 36);
  writeInt32(subChunk2Size, waveFileData, 40);  // SubChunk2Size (4)

  // Write actual audio data starting at offset 44.
  writeAudioBuffer(audioBuffer, waveFileData, 44, asFloat);

  return waveFileData;
}

function createAudioData(audioBuffer, asFloat) {
  return createWaveFileData(audioBuffer, asFloat);
}

function finishAudioTest(event) {
  let audioData = createAudioData(event.renderedBuffer);
  testRunner.setAudioData(audioData);
  testRunner.notifyDone();
}

// Save the given |audioBuffer| to a WAV file using the name given by
// |filename|.  This is intended to be run from a browser.  The
// developer is expected to use the console to run downloadAudioBuffer
// when necessary to create a new reference file for a test.  If
// |asFloat| is given and is true, the WAV file produced uses 32-bit
// float format (full WebAudio resolution).  Otherwise a 16-bit PCM
// WAV file is produced.
function downloadAudioBuffer(audioBuffer, filename, asFloat) {
  // Don't download if testRunner is defined; we're running a layout
  // test where this won't be useful in general.
  if (window.testRunner)
    return false;
  // Convert the audio buffer to an array containing the WAV file
  // contents.  Then convert it to a blob that can be saved as a WAV
  // file.
  let wavData = createAudioData(audioBuffer, asFloat);
  let blob = new Blob([wavData], {type: 'audio/wav'});
  // Manually create html tags for downloading, and simulate a click
  // to download the file to the given file name.
  let a = document.createElement('a');
  a.style.display = 'none';
  a.download = filename;
  let audioURL = window.URL.createObjectURL(blob);
  let audio = new Audio();
  audio.src = audioURL;
  a.href = audioURL;
  document.body.appendChild(a);
  a.click();
  return true;
}

