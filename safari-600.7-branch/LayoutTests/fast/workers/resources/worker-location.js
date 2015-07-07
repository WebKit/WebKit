function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

function gc()
{
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 10000; i++) { // > force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("abc");
    }
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var worker = createWorker();
worker.postMessage("eval WorkerLocation");
worker.postMessage("eval typeof location");
worker.postMessage("eval location");
worker.postMessage("eval location.href");
worker.postMessage("eval location.protocol");
worker.postMessage("eval location.host");
worker.postMessage("eval location.hostname");
worker.postMessage("eval location.port");
worker.postMessage("eval location.pathname");
worker.postMessage("eval location.search");
worker.postMessage("eval location.hash");
worker.postMessage("eval foo//bar");

worker.onmessage = function(evt) {
    if (!/foo\/\/bar/.test(evt.data))
        log(evt.data.replace(new RegExp("/.*LayoutTests"), "<...>"));
    else {
        log("DONE");
        if (window.testRunner)
            testRunner.notifyDone();
    }
}
