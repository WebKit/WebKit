class DynamicImportIsProhibitedProcessor extends AudioWorkletProcessor {
  process(inputs, outputs) {
    import("./dynamic-import-is-prohibited.js").then(() => {
        this.port.postMessage({
          error: null
        });
    }, (error) => {
        this.port.postMessage({
          error: String(error)
        });
    });
    return false;
  }
}

registerProcessor('dynamic-import-is-prohibited', DynamicImportIsProhibitedProcessor);
