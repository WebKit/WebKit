let ports = [];

onmessage = (event) => {
    ports.push(event.ports[0]);
}
