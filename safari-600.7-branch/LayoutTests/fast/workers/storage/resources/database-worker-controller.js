var databaseWorker = new Worker('resources/database-worker.js');

databaseWorker.onerror = function(event) {
    log("Caught an error from the worker!");
    log(event);
    for (var i in event)
        log("event[" + i + "]: " + event[i]);
};

databaseWorker.onmessage = function(event) {
    if (event.data.indexOf('log:') == 0) {
        log(event.data.substring(4));
    } else if (event.data == 'notifyDone') {
        if (window.testRunner)
            testRunner.notifyDone();
    } else if (event.data.indexOf('setLocationHash:') == '0') {
        location.hash = event.data.substring('setLocationHash:'.length);
    } else if (event.data == 'back') {
        history.back();
    } else
        throw new Error("Unrecognized message: " + event);
};

function log(message)
{
    document.getElementById("console").innerText += message + "\n";
}

function runTest(testFile)
{
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }
    document.getElementById("console").innerText = "";
    databaseWorker.postMessage("importScripts:../../../storage/websql/" + testFile);
    databaseWorker.postMessage("runTest");
}

