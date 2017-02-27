description("Test File.lastModifiedDate.");

jsTestIsAsync = true;

function onFileChange(file, filePath)
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "http://127.0.0.1:8000/resources/file-last-modified.php?path=../local/fileapi/" + filePath, false);
    xhr.send();
    var expectedDate = new Date(parseInt(xhr.responseText) * 1000);
    var actualDate = file.lastModified;
    if (expectedDate.getTime() == actualDate)
        testPassed("file.lastModified verified");
    else
        testFailed("file.lastModified incorrect");

    // Clean up after ourselves
    removeFileInputElement();

    finishJSTest();
}

function runTest()
{
    var testFilePath = "../resources/file-for-drag-to-send.txt";
    setFileInputChangeCallback(function(file) { onFileChange(file, testFilePath); });
    testRunner.setOpenPanelFiles([testFilePath]);

    var centerX = fileInput.offsetLeft + fileInput.offsetWidth / 2;
    var centerY = fileInput.offsetTop + fileInput.offsetHeight / 2;
    UIHelper.activateAt(centerX, centerY);
}

if (window.testRunner)
    runTest();
else
    testFailed("This test is not interactive, please run using a test harness");

var successfullyParsed = true;
