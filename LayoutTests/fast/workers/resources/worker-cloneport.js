onmessage = function(evt) {
    if (evt.data.indexOf("postBack ") == 0) {
        if (!evt.ports) {
            postMessage("FAILURE: No MessagePort with postBack command");
        }
        var strings = evt.data.split(" ", 2);
        var numItems = parseInt(strings[1]);
        postMessage("PASS: Received request for " + numItems + " messages");
        for (var i = 0 ; i < numItems ; i++) {
            var msg = "" + i;
            evt.ports[0].postMessage(msg);
        }
        postMessage("postBackDone");
    } else {
        postMessage("FAILURE: unknown message: " + evt.data);
    }
}
