function log(message)
{
    postMessage(message);
}

onmessage = function(event)
{
    var testFiles = event.data;
    log("Received files in worker");
    runNextTest(testFiles);
}
