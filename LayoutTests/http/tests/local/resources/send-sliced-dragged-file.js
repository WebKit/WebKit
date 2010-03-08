description("Test for slicing a dragged file and sending it via XMLHttpRequest.");

var originalText = "1234567890";
var tempFileName = "send-slice-dragged-file.tmp";
var tempFileOriginalModificationTime;
var subfile;

function uploadFile(file, expectedText, expectedException)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/xmlhttprequest/resources/post-echo.cgi", false);

    var passed;
    var message;
    try {
        xhr.send(file);
        if (expectedException) {
            passed = false;
            message = "Unexpected response data received: " + xhr.responseText + ". Expecting exception thrown";
        } else {
            if (xhr.responseText == expectedText) {
                passed = true;
                message = "Expected response data received: " + xhr.responseText;
            } else {
                passed = false;
                message = "Unexpected response data received: " + xhr.responseText + ". Expecting: " + expectedText;
            }
        }
    } catch (ex) {
        if (expectedException) {
            passed = true;
            message = "Expected exception thrown: " + ex;
        } else {
            passed = false;
            message = "Unexpected exception thrown: " + ex;
        }
    }

    if (passed)
        testPassed(message);
    else
        testFailed(message);
}

function createTempFile(fileData)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/resources/write-temp-file.php?filename=" + tempFileName + "&data=" + fileData, false);
    xhr.send();
    if (xhr.status != 200) {
        testFailed("Unexpected response status received: " + xhr.status);
        return;
    }

    var values = xhr.responseText.split('\n');
    if (values.length < 2) {
        testFailed("Unexpected response text received: " + xhr.responseText);
        return;
    }

    return values;
}

function touchTempFile()
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/resources/touch-temp-file.php?filename=" + tempFileName, false);
    xhr.send();
}

function removeTempFile()
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/resources/reset-temp-file.php?filename=" + tempFileName, false);
    xhr.send();
}

function onStableFileDrop(file, start, length)
{
    // Slice the file.
    subfile = file.slice(start, length);
    var expectedText = originalText.substring(start, start + length);
    shouldEvaluateTo("subfile.size", expectedText.length);

    // Upload the sliced file.
    uploadFile(subfile, expectedText, false);
}

function dragAndSliceStableFile(start, length)
{
    setFileInputDropCallback(function(file) { onStableFileDrop(file, start, length); });
    eventSender.beginDragWithFiles(["resources/file-for-drag-to-send.txt"]);
    moveMouseToCenterOfElement(fileInput);
    eventSender.mouseUp();
}

function onUnstableFileDrop(file, start, length)
{
    // Slice the file.
    subfile = file.slice(start, length);
    var expectedText = originalText.substring(start, start + length);
    shouldEvaluateTo("subfile.size", expectedText.length);
  
    // Upload the sliced file.
    uploadFile(subfile, expectedText, false);

    // Touch the underlying temp file.
    touchTempFile();
    
    // Upload the sliced file. We should receive an exception since the file has been changed.
    uploadFile(subfile, null, true);

    // Remove the temp file.
    removeTempFile();
}

function dragAndSliceUnstableFile(start, length)
{
    var tempFileInfo = createTempFile(originalText);
    if (tempFileInfo.length < 2) {
        done();
        return;
    }
    var tempFilePath = tempFileInfo[0];
    tempFileOriginalModificationTime = tempFileInfo[1];

    setFileInputDropCallback(function(file) { onUnstableFileDrop(file, start, length); });
    eventSender.beginDragWithFiles([tempFilePath]);
    moveMouseToCenterOfElement(fileInput);
    eventSender.mouseUp();
}

function runTest()
{
    dragAndSliceStableFile(2, 4);
    dragAndSliceStableFile(2, 20);
    dragAndSliceStableFile(15, 20);

    dragAndSliceUnstableFile(3, 5);
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    removeFileInputElement();
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
