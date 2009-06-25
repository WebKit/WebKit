description("Tests access to event.dataTransfer.files");

var dragTarget = document.createElement("div");
dragTarget.innerHTML = "Drag here"
dragTarget.style.backgroundColor = "blue";
dragTarget.style.width = "100px";
dragTarget.style.height = "100px";
// Important that we put this at the top of the doc so that logging does not cause it to go out of view (and be undragable)
document.body.insertBefore(dragTarget, document.body.firstChild);

dragTarget.addEventListener("dragentered", function() {
    event.dataTransfer.dropEffect = "copy";
    event.preventDefault();
}, false);

dragTarget.addEventListener("dragover", function() {
    debug("On dragover:")
    fileListShouldBe("event.dataTransfer.files", []);
    event.preventDefault();
}, false);

var expectedFilesOnDrop;
dragTarget.addEventListener("drop", function() {
    debug("On drop:")
    fileListShouldBe("event.dataTransfer.files", expectedFilesOnDrop);
    event.preventDefault();
}, false);

function moveMouseToCenterOfElement(element)
{
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function dragFilesOntoDragTarget(files) {
    eventSender.beginDragWithFiles(files);
    moveMouseToCenterOfElement(dragTarget);
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

function draggingPathsShouldResultInFiles(pathsArray, filesArray)
{
    expectedFilesOnDrop = filesArray;
    dragFilesOntoDragTarget(pathsArray);
}

function testDraggingFiles(filesArray)
{
    // We could make a way to parse the filename from the path, and then only need to pass
    // the path in the filesArray.
    var pathsOnly = filesArray.map(function(fileSpec) { return fileSpec['path']; });
    draggingPathsShouldResultInFiles(pathsOnly, filesArray);
}

function runTest()
{
    debug("Dragging no files should return an empty file list (arbitrary implementation detail):");
    testDraggingFiles([]);

    debug("Dragging a single (non-existant) file onto an element:");
    testDraggingFiles([
        { 'path': 'DRTFakeFile', 'name' : 'DRTFakeFile', 'size' : 0 }
    ]);

    debug("FIXME: File.fileSize always returns 0 for files dropped by eventSender.beginDragWithFiles from http tests:  https://bugs.webkit.org/show_bug.cgi?id=25909");

    debug("Dragging a real file onto an element:");
    testDraggingFiles([
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 }
    ]);

    debug("Dragging two files onto an element:");
    testDraggingFiles([
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 },
        { 'path': 'resources/mozilla.gif', 'name' : 'mozilla.gif', 'size' : 2593 }
    ]);

    debug("Dragging two files in reverse alphabetical order onto an element:");
    testDraggingFiles([
        { 'path': 'resources/mozilla.gif', 'name' : 'mozilla.gif', 'size' : 2593 },
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 }
    ]);

    // Directory dragging behavior is covered by https://bugs.webkit.org/show_bug.cgi?id=25852 and https://bugs.webkit.org/show_bug.cgi?id=25879
    debug("FIXME: We should not allow element to accept drops including directories unless https://bugs.webkit.org/show_bug.cgi?id=25879 is fixed to make directory File objects useful from JavaScript.  The page is given File objects corresponding to directories, but form submission and xhr.send() will fail.");
    debug("Dragging a directory onto an element:");
    draggingPathsShouldResultInFiles(['resources/directory-for-dragging'], []);

    // Note: The order of selection in the Finder changes the order of file paths in the pasteboard
    // thus it's important that we test different orders here as well (at least on the Mac)
    // Both drops should be refused or succeed based on how https://bugs.webkit.org/show_bug.cgi?id=25879 is resolved.  Currently we expect drops to be refused.
    debug("Dragging a file and a directory onto an element:");
    draggingPathsShouldResultInFiles(['resources/apple.gif', 'resources/directory-for-dragging'], []);

    debug("Dragging a directory and a file onto an element:")
    draggingPathsShouldResultInFiles(['resources/directory-for-dragging', 'resources/apple.gif'], []);
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    dragTarget.parentNode.removeChild(dragTarget);
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
