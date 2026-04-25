function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var worker = createWorker();

log("This tests that gc does not destroy the WorkerNavigator and WorkerLocation wrappers if the WorkerContext is still active. You should see two PASSes below if this test succeeds.");

worker.postMessage("eval navigator.foo = 'PASS'; gc(); navigator.foo");
worker.postMessage("eval location.foo = 'PASS'; gc(); location.foo");

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
