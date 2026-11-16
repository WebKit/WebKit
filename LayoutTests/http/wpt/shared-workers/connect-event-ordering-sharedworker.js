let ports = [];
onconnect = (e) => {
    const port = e.ports[0];
    ports.push(port);
    port.postMessage("worker" + ports.length);
}
