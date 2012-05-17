function generateError()
{
    // Generate an exception by accessing an undefined variable.
    foo.bar = 0;
}

onconnect = function(event) {
    event.ports[0].onmessage = function(evt) { handleMessage(evt, event.ports[0]); };
};

function gc()
{
    for (var i = 0; i < 10000; i++) { // force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("abc");
    }
}

function handleMessage(event, port) {
    if (event.data == "ping")
        port.postMessage("PASS: Received ping message");
    else if (event.data == "close")
        close();
    else if (event.data == "done")
        port.postMessage("DONE");
    else if (event.data == "throw")
        generateError();
    else if (event.data == "testingNameAttribute")
        port.postMessage(self.name);
    else if (/eval.+/.test(event.data)) {
        try {
            port.postMessage(event.data.substr(5) + ": " + eval(event.data.substr(5)));
        } catch (ex) {
            port.postMessage(event.data.substr(5) + ": " + ex);
        }
    }
    else
        port.postMessage("FAILURE: Received unknown message: " + event.data);
}
