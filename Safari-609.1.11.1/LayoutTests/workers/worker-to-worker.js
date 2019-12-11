self.onmessage = e => {
  e.data.port.onmessage = e => {
    self.postMessage(`[Worker] Received message: ${e.data}`);
  }
  e.data.port.postMessage('hello');
};
