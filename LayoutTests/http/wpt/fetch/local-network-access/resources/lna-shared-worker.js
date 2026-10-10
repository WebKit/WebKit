onconnect = (event) => {
    const port = event.ports[0];
    port.onmessage = async () => {
        const outcome = await fetch('http://127.0.0.1:8800/resources/testharness.js', { mode: 'no-cors' })
            .then(() => 'loaded', () => 'blocked');
        port.postMessage(outcome);
    };
};
