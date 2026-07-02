self.addEventListener("message", (event) => {
    if (!event.ports || !event.ports.length) {
        self.postMessage("FAIL: Message did not have a MessagePort");
        return;
    }
    event.ports[0].onmessage = echoBack;
    event.ports[0].start();
    self.postMessage("ready");
});

function echoBack(event) {
    self.postMessage("MessagePort echo: " + event.data);
}
