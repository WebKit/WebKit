onmessage = function(evt) {
    if (evt.data == "port") {
        if (evt.messagePort) {
            postMessage("PASS: Received message port");
            evt.messagePort.onmessage = pingBack;
            evt.messagePort.start();
        } else {
            postMessage("FAIL: Did not receive expected MessagePort");
        }
    } else if (evt.data == "noport") {
        if (evt.messagePort) {
            postMessage("FAIL: Received message port");
        } else {
            postMessage("PASS: evt.messagePort = null as expected");
        }
    } else if (evt.data == "spam") {
        for (var i = 0 ; i < 1000 ; i++) {
            evt.messagePort.postMessage(i);
        }
        postMessage("spamDone");
    } else if (evt.data == "getport") {
        var channel = new MessageChannel();
        postMessage("port", channel.port1);
        channel.port2.onmessage = pingBack;
        channel.port2.start();
    } else {
        postMessage("Unknown message:" + evt.data);
    }

}

function pingBack(evt) {
    // Make sure we got the expected data and send a return message over
    // the port.
    if (evt.data == "ping") {
        evt.target.postMessage("pong");
    } else {
        postMessage("FAIL: unknown message: " + evt.data);
    }
}
