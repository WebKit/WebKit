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
    if (event.ports) {
        event.source.postMessage("PASS: Got the MessagePort");
        event.ports[0].onmessage = echoBack;
        event.ports[0].start();
    } else 
        event.source.postMessage("FAIL: Message did not have a MessagePort");
});

function echoBack(evt) {
	client.postMessage("messageport thing received....");

    evt.target.postMessage("MessagePort echo: " + evt.data);
}
