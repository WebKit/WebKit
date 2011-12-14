function gc()
{
    for (var i = 0; i < 10000; i++) { // > force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("abc");
    }
}

function onmessage(evt)
{
    try {
        removeEventListener("message", onmessage, true);
        addEventListener("message", function(e) { e.foo = "bar" }, true);
        dispatchEvent(evt);
        postMessage((evt.foo == "bar") ? "SUCCESS" : "FAIL");
    } catch (ex) {
        postMessage(ex);
    }
}

addEventListener("message", onmessage, true);
gc();
