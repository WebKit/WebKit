onconnect = function(event) {
    event.messagePort.onmessage = function(evt) { handleMessage(evt, event.messagePort); };
};

function handleMessage(event, port) {
    if (event.data == "ping")
        port.postMessage("PASS: Received ping message");
    else if (event.data == "done")
        port.postMessage("DONE");
    else if (/eval.+/.test(evt.data)) {
        try {
            port.postMessage(evt.data.substr(5) + ": " + eval(evt.data.substr(5)));
        } catch (ex) {
            port.postMessage(evt.data.substr(5) + ": " + ex);
        }
    }
    else
        port.postMessage("FAILURE: Received unknown message: " + event.data);
}
