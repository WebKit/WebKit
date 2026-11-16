class BasicProcessor extends AudioWorkletProcessor {
  process(inputs, outputs) {
    this.port.postMessage("did rendering");
    return false;
  }
}

registerProcessor('basic-processor', BasicProcessor);
