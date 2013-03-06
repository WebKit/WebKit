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

function dumpResponse(xhr, fileSliced)
{
    debug(xhr.responseText);
}

function sendFormData(formDataList, fileSliced, sendAsAsync, addEventHandlers)
{
    var formData = new FormData();
    for (var i = 0; i < formDataList.length; i++) {
        if (formDataList[i].filename !== undefined)
            formData.append(formDataList[i].name, formDataList[i].value, formDataList[i].filename);
        else
            formData.append(formDataList[i].name, formDataList[i].value);
    }

    var xhr = new XMLHttpRequest();
    if (addEventHandlers)
        addEventHandlers(xhr, fileSliced);

    xhr.open("POST", "http://127.0.0.1:8000/xmlhttprequest/resources/multipart-post-echo.php", sendAsAsync);
    xhr.send(formData);

    if (!sendAsAsync)
        dumpResponse(xhr, fileSliced);
}

function testSendingFormData(dataList, sendAsAsync, addEventHandlers)
{
    var filesToDrag = [];
    for (var i = 0; i < dataList.length; i++) {
        if (dataList[i].type === 'file')
            filesToDrag.push(dataList[i].value);
    }

    if (filesToDrag.length !== 0) {
        eventSender.beginDragWithFiles(filesToDrag);
        moveMouseToCenterOfElement(fileInput);
        eventSender.mouseUp();
    }

    var files = fileInput.files;
    var formDataList = [];
    var fileSliced = false;
    for (var i = 0; i < dataList.length; i++) {
        var field = {name: dataList[i].name};
        if (dataList[i].type === 'file') {
            var fileName = getFileName(dataList[i].value);
            for (var j = 0; j < files.length; j++) {
                if (fileName == files[j].name) {
                    var file = files[j];
                    if ('start' in dataList[i] && 'length' in dataList[i]) {
                        fileSliced = true;
                        file = file.slice(dataList[i].start, dataList[i].start + dataList[i].length);
                    }
                    field.value = file;
                    break;
                }
            }
        }
        else
            field.value = dataList[i].value;
        if (dataList[i]['filename'])
            field.filename = dataList[i].filename;

        formDataList.push(field);
    }

    sendFormData(formDataList, fileSliced, sendAsAsync, addEventHandlers);
}

function formDataTestingCleanup()
{
    // Clean up after ourselves
    fileInput.parentNode.removeChild(fileInput);
}
