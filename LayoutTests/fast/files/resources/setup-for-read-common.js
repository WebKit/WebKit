function log(message)
{
    document.getElementById('console').appendChild(document.createTextNode(message + "\n"));
    if (message == "DONE") {
        if (window && window.testRunner)
            testRunner.notifyDone();
    }
}

function onInputFileChange(testFileInfoList)
{
    var files = document.getElementById("file").files;
    var testFiles = { };
    for (var i = 0; i < files.length; i++)
        testFiles[testFileInfoList[i]['name']] = files[i];

    startTest(testFiles);
}

function runTests(testFileInfoList)
{
    var pathsOnly = testFileInfoList.map(function(fileSpec) { return fileSpec['path']; });
    eventSender.beginDragWithFiles(pathsOnly);
    eventSender.mouseMoveTo(10, 10);
    eventSender.mouseUp();
}

function startWorker(testFiles, workerScriptURL)
{
    var worker = new Worker(workerScriptURL);
    worker.onmessage = function(event)
    {
        log(event.data);
        if (event.data == "DONE") {
            if (window.testRunner)
                testRunner.notifyDone();
        }
    }
    worker.onerror = function(event)
    {
        log("Received error from worker: " + event.message);
        if (window.testRunner)
            testRunner.notifyDone();
    }
    worker.postMessage(testFiles);
}
