self.addEventListener("message", (event) => {
    event.source.postMessage("Service worker received message '" + event.data + "' from origin '" + event.origin + "'");
});

