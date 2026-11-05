onconnect = event => {
    const port = event.ports[0];
    port.start();
    port.postMessage("connected");
};
