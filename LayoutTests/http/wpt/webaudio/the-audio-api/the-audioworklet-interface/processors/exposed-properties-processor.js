class ExposedPropertiesTestProcessor extends AudioWorkletProcessor {
  process(inputs, outputs) {
    this.port.postMessage({
      exposed_properties: Object.getOwnPropertyNames(globalThis)
    });
    return false;
  }
}

registerProcessor('exposed-properties-test', ExposedPropertiesTestProcessor);
