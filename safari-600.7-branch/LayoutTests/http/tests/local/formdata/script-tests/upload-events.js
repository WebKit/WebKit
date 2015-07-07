description("Test that upload events are fired when sending FormData via XMLHttpRequest.");

var progressEventFired = false;
function progressHandler(evt)
{
    // Dump the progress event only once in order to get a consistent result.
    if (progressEventFired)
        return;
    progressEventFired = true;

    debug("onprogress event fired on XMLHttpRequestUpload: " + evt);
}

function loadstartHandler(evt)
{
    debug("loadstart event fired on XMLHttpRequestUpload: " + evt);
}

function loadHandler(evt)
{
    debug("load event fired on XMLHttpRequestUpload: " + evt);
}

function readystatechangeHandler(evt, xhr, fileSliced)
{
    if (xhr.readyState == xhr.DONE) {
        dumpResponse(xhr, fileSliced);
        finishJSTest();
    }
}

function addXHREventHandlers(xhr, fileSliced)
{
    xhr.upload.onprogress = progressHandler;
    xhr.upload.onloadstart = loadstartHandler;
    xhr.upload.onload = loadHandler;
    xhr.onreadystatechange = function(evt) { readystatechangeHandler(evt, xhr, fileSliced); };
}

function runTest()
{
    testSendingFormData(
        [
            { 'type': 'string', 'name': 'string1', 'value': 'foo' },
            { 'type': 'file', 'name': 'file1', 'value': '../resources/file-for-drag-to-send.txt' }
        ],
        true,
        addXHREventHandlers
    );
}

if (window.eventSender) {
    window.jsTestIsAsync = true;
    runTest();
    formDataTestingCleanup();
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
