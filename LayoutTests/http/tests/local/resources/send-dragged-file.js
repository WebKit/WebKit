description("Test for sending a dragged file via XMLHttpRequest.");

var fileInput = document.createElement("input");
fileInput.type = "file";
fileInput.style.width = "100px";
fileInput.style.height = "100px";
// Important that we put this at the top of the doc so that logging does not cause it to go out of view (where it can't be dragged to)
document.body.insertBefore(fileInput, document.body.firstChild);

fileInput.addEventListener("dragenter", function() {
    event.preventDefault();
}, false);

fileInput.addEventListener("dragover", function() {
    event.preventDefault();
}, false);

fileInput.addEventListener("drop", function() {
    if (event.dataTransfer.types.indexOf("Files") != -1 && event.dataTransfer.files.length == 1)
        testPassed("event.dataTransfer contains a File object on drop.");
    else {
        testFailed("event.dataTransfer does not contain a File object on drop.");
        return;
    }

    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/xmlhttprequest/resources/post-echo.cgi", false);
    xhr.send(event.dataTransfer.files[0]);
    if (xhr.responseText == "Hello")
        testPassed("Expected response data received.");
    else
        testFailed("Unexpected response data received: " + xhr.responseText);

    event.preventDefault();
}, false);

function moveMouseToCenterOfElement(element) {
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function runTest()
{
    eventSender.beginDragWithFiles(["resources/file-for-drag-to-send.txt"]);
    moveMouseToCenterOfElement(fileInput);
    eventSender.mouseUp();
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    fileInput.parentNode.removeChild(fileInput);
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
