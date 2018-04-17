self.onmessage = e => {
  const {name} = e.data;

  e.data.port.onmessage = e => {
    self.postMessage(`[${name}] Received message: ${e.data}`);
  }

  const sendMsg = () => e.data.port.postMessage('hello');
  if (e.data.wait) setTimeout(sendMsg, e.data.wait);
  else sendMsg();
};
