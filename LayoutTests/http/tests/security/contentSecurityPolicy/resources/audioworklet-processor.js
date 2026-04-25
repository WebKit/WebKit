class DummyProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process(inputs, outputs, parameters) {
    // Doesn't do anything here.
    return true;
  }
}

registerProcessor('dummy', DummyProcessor);
