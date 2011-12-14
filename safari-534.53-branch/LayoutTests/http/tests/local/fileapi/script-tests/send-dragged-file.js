description("Test for sending a dragged file via XMLHttpRequest.");

function onFileDrop(file)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/xmlhttprequest/resources/post-echo.cgi", false);
    xhr.send(file);
    if (xhr.responseText == "1234567890")
        testPassed("Expected response data received.");
    else
        testFailed("Unexpected response data received: " + xhr.responseText);

    event.preventDefault();
}

function runTest()
{
    setFileInputDropCallback(onFileDrop);
    eventSender.beginDragWithFiles(["../resources/file-for-drag-to-send.txt"]);
    moveMouseToCenterOfElement(fileInput);
    eventSender.mouseUp();
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    removeFileInputElement();
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
