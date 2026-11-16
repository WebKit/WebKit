// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end task 2: Speech recognition.
// Based on the example https://huggingface.co/Xenova/whisper-tiny.en
// Convert audio inputs first with `convert-audio.mjs`.

globalThis.initPipeline = async function(pipeline) {
  return await pipeline(
    'automatic-speech-recognition',
    'Xenova/whisper-tiny.en',
    // Use quantized model because of smaller weights.
    { dtype: 'q8' }
  );
}

globalThis.doTask = async function(pipeline, inputFileBuffer) {
  const input = new Float32Array(inputFileBuffer);
  const output = await pipeline(input);
  return output.text.trim();
}

globalThis.validate = function(output) {
  if (output !== 'Ask not what your country can do for you.') {
    throw new Error('Wrong output, got: ' + output);
  }
}
