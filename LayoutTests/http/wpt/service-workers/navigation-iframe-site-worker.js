addEventListener("message", (e) => {
    self.port = e.data.port;
});

addEventListener("fetch", (e) => {
    if (self.internals) {
        self.port.postMessage(self.internals.fetchEventIsSameSite(e) ? "FAIL" : "PASS");
        return;
    }
    self.port.postMessage("PASS");
});
