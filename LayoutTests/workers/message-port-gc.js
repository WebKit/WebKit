(function() {
  let {port1, port2} = new MessageChannel();
  self.postMessage({ port: port2 }, [port2]);
  port1.onmessage = e => {
    for (let i = 0; i < 1000; i++)
      new ArrayBuffer(500);
    setTimeout(() => port1.postMessage("pong"), 50);
  };
})();
