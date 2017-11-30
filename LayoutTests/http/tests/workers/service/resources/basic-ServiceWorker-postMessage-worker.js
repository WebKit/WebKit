var client = null;
self.addEventListener("message", (event) => {
    if (!client) {
        client = event.source;
        if (!(client instanceof WindowClient)) {
            event.source.postMessage("FAIL: client source is not a WindowClient");
            return;
        }
    } else if (client !== event.source) {
        event.source.postMessage("FAIL: client source of the second message is not the same as the first message");
        return;
    }
    event.source.postMessage("PASS: Service worker received message '" + event.data + "' from origin '" + event.origin + "'");
});

