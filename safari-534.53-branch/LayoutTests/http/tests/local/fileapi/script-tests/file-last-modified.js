description("Test File.lastModifiedDate.");

function onFileDrop(file, filePath)
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "http://127.0.0.1:8000/resources/file-last-modified.php?path=../local/fileapi/" + filePath, false);
    xhr.send();
    var expectedDate = new Date(parseInt(xhr.responseText) * 1000);
    var actualDate = file.lastModifiedDate;
    if (expectedDate.toString() == actualDate.toString())
        testPassed("file.lastModifiedDate verified");
    else
        testFailed("file.lastModifiedDate incorrect");
}

function runTest()
{
    var testFilePath = "../resources/file-for-drag-to-send.txt";
    setFileInputDropCallback(function(file) { onFileDrop(file, testFilePath); });
    eventSender.beginDragWithFiles([testFilePath]);
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
