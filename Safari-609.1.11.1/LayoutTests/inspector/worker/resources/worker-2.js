onmessage = function(event) {
    let echo = `Worker 2 Echo: ${event.data}`;
    postMessage(echo);
}
