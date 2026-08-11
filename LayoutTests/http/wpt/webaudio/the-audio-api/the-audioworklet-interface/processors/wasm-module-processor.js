/**
 * @class WASMModuleProcessor
 * @extends AudioWorkletProcessor
 *
 * This processor class demonstrates passing WASM modules to and from
 * audio worklets.
 */
class WASMModuleProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.port.onmessage = this.handleMessage.bind(this);
    this.port.onmessageerror = this.handleMessageError.bind(this);
    this.port.postMessage({state: 'created'});
  }

  testModule(module) {
    try {
      let instance = new WebAssembly.Instance(module);
      let increment = instance.exports["increment"];
      if (typeof increment != "function")
        return false;
      let result = increment(42);
      return result == 43;
    } catch (e) {
      return false;
    }
  }

  handleMessage(event) {
    this.port.postMessage({
      state: 'received message',
      isValidModule: this.testModule(event.data.module)
    });
  }

  handleMessageError(event) {
    this.port.postMessage({
      state: 'received messageerror'
    });
  }

  process() {
    return true;
  }
}

registerProcessor('wasm-module-processor', WASMModuleProcessor);
