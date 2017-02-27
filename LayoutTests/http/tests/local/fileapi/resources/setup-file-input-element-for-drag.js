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
    event.dataTransfer.dropEffect = "copy";
    event.preventDefault();
}, false);

var fileInputDropCallback = null;
fileInput.addEventListener("drop", function() {
    if (event.dataTransfer.types.indexOf("Files") != -1 && event.dataTransfer.files.length == 1)
        testPassed("event.dataTransfer contains a File object on drop.");
    else {
        testFailed("event.dataTransfer does not contain a File object on drop.");
        return;
    }

    fileInputDropCallback(event.dataTransfer.files[0]);

    event.preventDefault();
}, false);

var fileInputChangeCallback = null;
fileInput.addEventListener("change", function() {
    if (event.target.files.length == 1)
        testPassed("event.target.files contains a File object.");
    else
        testPassed("event.target.files does not contain a File object.");

    if (fileInputChangeCallback)
        fileInputChangeCallback(event.target.files[0]);
}, false);

function setFileInputDropCallback(fileInputDropCallbackFunc)
{
    fileInputDropCallback = fileInputDropCallbackFunc;
}

function setFileInputChangeCallback(fileInputChangeCallbackFunc)
{
    fileInputChangeCallback = fileInputChangeCallbackFunc;
}

function moveMouseToCenterOfElement(element)
{
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function removeFileInputElement()
{
    fileInput.parentNode.removeChild(fileInput)
}
