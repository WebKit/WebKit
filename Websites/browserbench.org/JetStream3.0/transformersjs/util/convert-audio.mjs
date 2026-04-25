// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Convert an WAV audio file to a format that Transformers.js Whisper model can
// work with.
// Write the output as raw float values, so we don't need any additional
// WAV/RIFF parser but can directly use it.
// See https://huggingface.co/docs/transformers.js/guides/node-audio-processing

import wavefile from 'wavefile';
import * as fs from 'fs';

if (process.argv.length < 5) {
  throw new Error("Usage: node convert-audio.mjs input.wav output.raw <start_sample> <end_sample>");
}
const inputFilename = process.argv[2];
const outputFilename = process.argv[3];
const outputStartSample = parseInt(process.argv[4]);
const outputEndSample = parseInt(process.argv[5]);

const wav = new wavefile.WaveFile(fs.readFileSync(inputFilename));
const inputChannelCount = wav.fmt.numChannels;
const inputSampleRate = wav.fmt.sampleRate;
const inputBitDepth = wav.fmt.bitsPerSample;

// Pipeline expects input as a Float32Array.
wav.toBitDepth('32f');

// Whisper expects audio with a sampling rate of 16000.
const outputSampleRate = 16000;
wav.toSampleRate(outputSampleRate);
// Explicitly set output type to `Float32Array` to make the raw output smaller.
// (Otherwise, `Float64Array` is the default.)
let audioData = wav.getSamples(/* interleaved */ false, Float32Array);

// Downmix to mono, if necessary.
const outputChannelCount = 1;
if (Array.isArray(audioData)) {
  if (audioData.length > 1) {
    const SCALING_FACTOR = Math.sqrt(2);
    for (let i = 0; i < audioData[0].length; ++i) {
      audioData[0][i] = SCALING_FACTOR * (audioData[0][i] + audioData[1][i]) / 2;
    }
  }
  audioData = audioData[0];
}

const inputSampleCount = audioData.length;

// Select samples as specified by outputStartSample and outputEndSample.
console.assert(outputStartSample < outputEndSample, "Given start is smaller than end.");
console.assert(outputEndSample <= audioData.length, "Given end is longer than input file.");
audioData = audioData.slice(outputStartSample, outputEndSample);

const outputSampleCount = audioData.length;
const outputSizeBytes = audioData.byteLength;
fs.writeFileSync(outputFilename, audioData);

const durationSeconds = outputSampleCount / outputSampleRate;
console.log(`Converted ${durationSeconds}s of audio`);
console.log(`  from '${inputFilename}', ${inputChannelCount} channel(s), ${inputSampleRate} Hz, ${inputBitDepth} bit, ${inputSampleCount} samples`);
console.log(`  to   '${outputFilename}', ${outputChannelCount} channel(s), ${outputSampleRate} Hz, 32 bit float, ${outputSampleCount} samples, ${outputSizeBytes} bytes`);
