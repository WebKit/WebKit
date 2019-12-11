
function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var worker = createWorker();
worker.postMessage("eval self.MessageEvent = 'PASS'; MessageEvent;");
worker.postMessage("eval foo//bar");

worker.onmessage = function(evt) {
    if (!/foo\/\/bar/.test(evt.data))
        log(evt.data);
    else {
        log("DONE");
        if (window.testRunner)
            testRunner.notifyDone();
    }
}
