function gc()
{
    for (var i = 0; i < 10000; i++) { // > force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("abc");
    }
}

function onmessage(evt)
{
    postMessage("SUCCESS");
}

addEventListener("message", onmessage, true);
gc();
