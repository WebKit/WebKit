description("Test File.lastModifiedDate returns null if the information is not available.");

var tempFileContent = "1234567890";
var tempFileName = "file-last-modified-after-delete.tmp";
var testStartTime = new Date();
var lastModified;

function onFileDrop(file)
{
    // Remove the temp file.
    removeTempFile(tempFileName);

    // This synchronosly queries the file's lastModified (which should fail) until/unless we start capturing the file metadata at File construction time.
    lastModified = file.lastModified;

    // The returned value should be equal to the current date/time since the file's modified date/time is not available.
    shouldNotBe('lastModified', 'null');
    shouldBeGreaterThanOrEqual('lastModified', 'testStartTime.getTime()');
    shouldBeGreaterThanOrEqual('(new Date()).getTime()', 'lastModified');
}

function runTest()
{
    var tempFilePath = createTempFile(tempFileName, tempFileContent);
    if (tempFilePath.length == 0)
        return;

    setFileInputDropCallback(onFileDrop);
    eventSender.beginDragWithFiles([tempFilePath]);
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
