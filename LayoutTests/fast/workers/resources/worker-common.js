function gc()
{
    for (var i = 0; i < 10000; i++) { // > force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("abc");
    }
}

onmessage = function(evt)
{
    gc();

    if (evt.data == "ping")
        postMessage("pong");
    else if (evt.data == "freeze")
        while (1) {}
    else if (/eval.+/.test(evt.data)) {
        try {
            postMessage(evt.data.substr(5) + ": " + eval(evt.data.substr(5)));
        } catch (ex) {
            postMessage(evt.data.substr(5) + ": " + ex);
        }
    }
    gc();
}
