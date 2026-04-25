/**
 * @class MessagePortProcessor
 * @extends AudioWorkletProcessor
 *
 * This processor class demonstrates passing a MessagePort to audio worklets.
 */
class MessagePortProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.port.onmessage = this.handleMessage.bind(this);
    this.port.onmessageerror = this.handleMessageError.bind(this);
    this.port.postMessage({state: 'created'});
  }

  handleMessage(event) {
    event.data.port.postMessage({
      state: 'port-received',
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

registerProcessor('message-port-processor', MessagePortProcessor);
