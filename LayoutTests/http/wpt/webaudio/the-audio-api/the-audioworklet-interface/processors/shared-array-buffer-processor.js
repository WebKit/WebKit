class SharedArrayBufferTestProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.sab = options.processorOptions['buffer'];
    this.port.onmessage = (e) => {
        this.port.postMessage({ 'sab_value': new Int16Array(this.sab)[0] });
    };
    this.port.postMessage({ 'sab_value': new Int16Array(this.sab)[0] });
  }

  process(inputs, outputs) {
    return true;
  }
}

registerProcessor('shared-array-buffer-test', SharedArrayBufferTestProcessor);
