var client = null;
let receivedState = false;

self.addEventListener("message", (event) => {
    if (event.data === "SetState") {
        receivedState = true;
        return;
    }

    if (event.data === "HasState") {
        event.source.postMessage(receivedState);
        return;
    }
});

