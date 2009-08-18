if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

description("This test checks whether exceptions in SharedWorkers are logged to the parent document. An exception should be logged to the error console.");

var worker = createWorker();
worker.postMessage("throw");
worker.postMessage("ping");

// Wait for response from ping - that's how we know we have thrown the exception.
worker.onmessage = function(event)
{
    debug(event.data);
    done();
};

function done()
{
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}
