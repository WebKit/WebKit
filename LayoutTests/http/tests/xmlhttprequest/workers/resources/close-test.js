if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

var timeout = 0;
// Start with async tests
testAsync();

function testAsync() {
    var worker = createWorker('resources/close.js');
    worker.onerror = handleException;
    worker.postMessage("async");
    worker.onmessage = function(evt)
    {
        if (/DONE/.test(evt.data)) {
            // Give worker a chance to complete the shutdown
            setTimeout(function() {
                log("PASS: Async test");
                testSync();
            }, 1000);
        } else {
            log("ERROR");
            done();
        }
    }
}

function testSync() {
    var worker = createWorker('resources/close.js');
    worker.onerror = handleException;
    worker.postMessage("sync");
    worker.onmessage = function(evt)
    {
        if (/DONE/.test(evt.data)) {
            // Give worker a chance to complete the shutdown
            setTimeout(function() {
                log("PASS: sync test");
                log("DONE");
                done();
            }, 1000);
        } else {
            log("ERROR");
            done();
        }
    }
}

function done()
{
    clearTimeout(timeout);
    if (window.testRunner)
        testRunner.notifyDone();
}

function handleException(evt)
{
    log("ERROR - exception thrown: " + evt.message);
    done();
}
