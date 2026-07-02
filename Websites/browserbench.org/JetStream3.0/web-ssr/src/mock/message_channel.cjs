class MessagePort {
  constructor() {
    this._otherPort = null;
    this._listeners = new Set();
    this.onmessage = null;
  }

  _link(port) {
    this._otherPort = port;
  }

  _dispatchMessage(data) {
    const event = { data };
    queueMicrotask(() => {
      if (typeof this.onmessage === 'function') {
        this.onmessage(event);
      }
      for (const listener of this._listeners) {
        listener(event);
      }
    });
  }

  postMessage(data) {
    if (!this._otherPort) {
      throw new Error("Port is not connected.");
    }
    const clonedData = JSON.parse(JSON.stringify(data));
    this._otherPort._dispatchMessage(clonedData);
  }

  addEventListener(eventName, listener) {
    if (eventName === 'message' && typeof listener === 'function') {
      this._listeners.add(listener);
    }
  }

  removeEventListener(eventName, listener) {
    if (eventName === 'message') {
      this._listeners.delete(listener);
    }
  }

  start() { }
  close() { }
}

class MessageChannel {
  constructor() {
    this.port1 = new MessagePort();
    this.port2 = new MessagePort();
    this.port1._link(this.port2);
    this.port2._link(this.port1);
  }
}

module.exports = {
  MessageChannel,
};