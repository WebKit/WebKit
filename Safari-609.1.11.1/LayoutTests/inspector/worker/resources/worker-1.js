onmessage = function(event) {
    let echo = `Worker 1 Echo: ${event.data}`;
    postMessage(echo);
}

passphrase = "worker-passphrase";
