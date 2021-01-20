self.addEventListener("message", (event) => {
    event.source.postMessage(self.internals ? internals.processIdentifier : "needs internal API");
});
