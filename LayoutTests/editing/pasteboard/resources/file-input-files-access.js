description("Tests for multi-file drag onto file input elements for https://bugs.webkit.org/show_bug.cgi?id=25862");

var fileInput = document.createElement("input");
fileInput.type = 'file';
fileInput.style.width = "100%"; // So that any manual testing will show full file names
document.body.appendChild(fileInput);

function moveMouseToCenterOfElement(element)
{
    var centerX = fileInput.offsetLeft + fileInput.offsetWidth / 2;
    var centerY = fileInput.offsetTop + fileInput.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function dragFilesOntoInput(files) {
    fileInput.value = ""; // Clear the <input>

    eventSender.beginDragWithFiles(files);
    moveMouseToCenterOfElement(fileInput);
    eventSender.mouseUp();
}

function fileListShouldBe(fileListString, filesArray)
{
    shouldBe(fileListString + ".length", "" + filesArray.length);
    for (var x = 0; x < filesArray.length; x++) {
        var fileValueString = fileListString + "[" + x + "]";
        shouldBeEqualToString(fileValueString + ".fileName", filesArray[x]['name']);
        shouldBe(fileValueString + ".fileSize", "" + filesArray[x]['size']);
    }
}

function filesShouldBe(filesArray)
{
    fileListShouldBe("fileInput.files", filesArray);
}

function draggingPathsShouldResultInFiles(pathsArray, filesArray)
{
    dragFilesOntoInput(pathsArray);
    // WebKit seems to always take the first file in the dragged list as .value:
    shouldBeEqualToString("fileInput.value", filesArray[0] ? filesArray[0]['name'] : '');
    filesShouldBe(filesArray);
}

function testDraggingFiles(filesArray)
{
    // We could make a way to parse the filename from the path, and then only need to pass
    // the path in the filesArray.
    var pathsOnly = filesArray.map(function(fileSpec) { return fileSpec['path']; });
    draggingPathsShouldResultInFiles(pathsOnly, filesArray);
}

function testOrderedDraggingWithDirectory()
{
    var inputType = fileInput.multiple ? "mutli-file" : "single-file";

    // Note: The order of selection in the Finder changes the order of file paths in the pasteboard
    // thus it's important that we test different orders here as well (at least on the Mac)
    debug("Dragging a file and a directory onto a " + inputType + " input control:")
    draggingPathsShouldResultInFiles(['resources/apple.gif', 'resources/directory-for-dragging'], []);

    debug("Dragging a directory and a file onto a "  + inputType + " input control:")
    draggingPathsShouldResultInFiles(['resources/directory-for-dragging', 'resources/apple.gif'], []);
}

function runTest()
{
    debug("Dragging to a disabled file input control:")
    fileInput.disabled = true;
    draggingPathsShouldResultInFiles(['DRTFakeFile'], [])
    fileInput.disabled = false;

    debug("Dragging a single (non-existant) file to a file input control:")
    testDraggingFiles([
        { 'path': 'DRTFakeFile', 'name' : 'DRTFakeFile', 'size' : 0 }
    ]);

    debug("Dragging a real file to a file input control:")
    testDraggingFiles([
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 }
    ]);

    debug("Dragging a file with URL encoded characters to a file input control:")
    testDraggingFiles([
        { 'path': 'resources/File With Spaces! For Drägging?.gif', 'name' : 'File With Spaces! For Drägging?.gif', 'size' : 1476 }
    ]);

    // Directory dragging behavior is covered by
    // https://bugs.webkit.org/show_bug.cgi?id=25852
    debug("Dragging a directory onto an file input control:")
    draggingPathsShouldResultInFiles(['resources/directory-for-dragging'], []);

    // FIXME: Current behavior is to take the first file, but I expect that's
    // confusing to the user.  We should change this to expect drag failure.
    debug("Dragging two files to a single-file input control:")
    draggingPathsShouldResultInFiles(['resources/apple.gif', 'resources/mozilla.gif'], [{ 'name' : 'apple.gif', 'size' : 1476 }]);

    testOrderedDraggingWithDirectory();

    fileInput.multiple = true;

    debug("Dragging two files to a multi-file input control:")
    testDraggingFiles([
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 },
        { 'path': 'resources/mozilla.gif', 'name' : 'mozilla.gif', 'size' : 2593 }
    ]);

    testOrderedDraggingWithDirectory();
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    fileInput.parentNode.removeChild(fileInput);
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}


var successfullyParsed = true;
