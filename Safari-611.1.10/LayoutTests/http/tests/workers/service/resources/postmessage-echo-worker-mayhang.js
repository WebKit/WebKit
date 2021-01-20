self.addEventListener("message", (event) => {
    event.source.postMessage("PASS: Service worker received message '" + event.data + "'");

    if (event.data === "HANG") {
        while(1) { };
    }
});

