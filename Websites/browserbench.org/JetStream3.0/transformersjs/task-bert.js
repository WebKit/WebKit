// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end task 1: Sentiment analysis, so NLP.

globalThis.initPipeline = async function(pipeline) {
  return await pipeline(
    'sentiment-analysis',
    'Xenova/distilbert-base-uncased-finetuned-sst-2-english',
    // Use quantized models for smaller model weights.
    { dtype: 'uint8' }
  );
}

globalThis.doTask = async function(pipeline) {
  const inputs = [
    'I love transformers!',
    'Benchmarking is hard.',
  ];
  const outputs = await pipeline(inputs);
  return outputs;
}

globalThis.validate = function(outputs) {
  if (outputs.length !== 2) {
    throw new Error('Expected output to be an array matching the inputs, but got:' + outputs);
  }
  if (outputs[0].label !== 'POSITIVE' || outputs[0].score < 0.9) {
    throw new Error('Expected positive sentiment for first input, but got: ' + outputs[0]);
  }
  if (outputs[1].label !== 'NEGATIVE' || outputs[1].score < 0.9) {
    throw new Error('Expected negative sentiment for second input, but got: ' + outputs[1]);
  }
}
