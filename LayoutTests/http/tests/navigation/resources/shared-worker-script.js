var channel = new BroadcastChannel('shared-worker');
var counter = 0;
var state = 'empty';
channel.onmessage = (event) => {
    if (event.data === 'ping') {
        counter++;
        channel.postMessage('pong');
    }
}
self.onconnect = function(e) {
    const port = e.ports[0];
    port.onmessage = (event) => {
        if (event.data === 'counter') {
            port.postMessage(counter);
            return;
        }
        if (event.data === 'getState') {
            port.postMessage(state);
            return;
        }
        if (event.data === 'ping') {
            port.postMessage('pong');
            return;
        }
        if (event.data.action === 'setState') {
            state = event.data.state;
            port.postMessage('ok');
            return;
        }
    };
}
