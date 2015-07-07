if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

description("This test checks whether exceptions in SharedWorkers are logged to the parent document. An exception should be logged to the error console.");

var worker = createWorker();
worker.postMessage("throw");
worker.postMessage("ping");

// Wait for response from ping - that's how we know we have thrown the exception.
worker.onmessage = function(event)
{
    debug(event.data);

    // Give the console message a chance to be written out before ending the test (timers are processed after the task queue is empty).
    setTimeout(done, 0);
};

function done()
{
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    if (window.testRunner)
        testRunner.notifyDone();
}
