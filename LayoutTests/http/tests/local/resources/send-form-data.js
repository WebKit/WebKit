description("Test for sending FormData via XMLHttpRequest.");

var fileInput = document.createElement("input");
fileInput.type = 'file';
fileInput.style.width = "100%"; // So that any manual testing will show full file names
// Important that we put this at the top of the doc so that logging does not cause it to go out of view (where it can't be dragged to)
document.body.insertBefore(fileInput, document.body.firstChild);

function moveMouseToCenterOfElement(element)
{
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function getFileName(filePath)
{
    var index = filePath.lastIndexOf('/');
    if (index == -1)
        return filePath;
    return filePath.substr(index + 1);
}

function sendFormData(formDataList)
{
    var formData = new FormData();
    for (var i = 0; i < formDataList.length; i++)
        formData.append(formDataList[i]['name'], formDataList[i]['value']);

    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/xmlhttprequest/resources/multipart-post-echo.php", false);
    xhr.send(formData);

    debug(xhr.responseText);
}

function testSendingFormData(dataList)
{
    var filesToDrag = [];
    for (var i = 0; i < dataList.length; i++) {
        if (dataList[i]['type'] == 'file')
            filesToDrag.push(dataList[i]['value']);
    }

    if (filesToDrag) {
        eventSender.beginDragWithFiles(filesToDrag);
        moveMouseToCenterOfElement(fileInput);
        eventSender.mouseUp();
    }

    var files = fileInput.files;
    var formDataList = [];
    for (var i = 0; i < dataList.length; i++) {
        if (dataList[i]['type'] == 'file') {
            var fileName = getFileName(dataList[i]['value']);
            for (var j = 0; j < files.length; j++) {
                if (fileName == files[j].name) {
                    formDataList.push({'name': dataList[i]['name'], 'value': files[j]});
                    break;
                }
            }
        } else {
            formDataList.push({'name': dataList[i]['name'], 'value': dataList[i]['value']});
        }
    }

    sendFormData(formDataList);
}

function runTest()
{
    debug("Sending FormData containing one string with empty name:");
    testSendingFormData([
        { 'type': 'string', 'name': '', 'value': 'foo' }
    ]);

    debug("Sending FormData containing one file with empty name:");
    testSendingFormData([
        { 'type': 'file', 'name': '', 'value': 'resources/file-for-drag-to-send.txt' }
    ]);

    debug("Sending FormData containing one string:");
    testSendingFormData([
        { 'type': 'string', 'name': 'string', 'value': 'foo' }
    ]);

    debug("Sending FormData containing one file:");
    testSendingFormData([
        { 'type': 'file', 'name': 'file', 'value': 'resources/file-for-drag-to-send.txt' }
    ]);

    debug("Sending FormData containing one string and one file:");
    testSendingFormData([
        { 'type': 'string', 'name': 'string1', 'value': 'foo' },
        { 'type': 'file', 'name': 'file1', 'value': 'resources/file-for-drag-to-send.txt' }
    ]);

    debug("Sending FormData containing two strings and two files:");
    testSendingFormData([
        { 'type': 'string', 'name': 'string1', 'value': 'foo' },
        { 'type': 'file', 'name': 'file1', 'value': 'resources/file-for-drag-to-send.txt' },
        { 'type': 'string', 'name': 'string2', 'value': 'bar' },
        { 'type': 'file', 'name': 'file2', 'value': 'resources/file-for-drag-to-send.txt' }
    ]);
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    fileInput.parentNode.removeChild(fileInput);
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
