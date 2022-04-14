var channel = new BroadcastChannel('shared-worker');
var counter = 0;
channel.onmessage = (event) => {
    if (event.data === 'ping') {
        counter++;
        channel.postMessage('pong');
    }
}
self.onconnect = function(e) {
    const port = e.ports[0];
    port.onmessage = (event) => {
        if (event.data === 'counter')
            port.postMessage(counter);
    };
}
