description("Tests access to event.dataTransfer.files and .types");

var dragTarget = document.createElement("div");
dragTarget.innerHTML = "Drag here"
dragTarget.style.backgroundColor = "blue";
dragTarget.style.width = "100px";
dragTarget.style.height = "100px";
// Important that we put this at the top of the doc so that logging does not cause it to go out of view (and be undragable)
document.body.insertBefore(dragTarget, document.body.firstChild);

var filesToDrag;
dragTarget.addEventListener("dragenter", function() {
    debug("On dragenter:")
    event.dataTransfer.dropEffect = "copy";
    var shouldContainType = (filesToDrag.length > 0);
    checkForEventTransferType(event, "Files", shouldContainType);
    fileListShouldBe("event.dataTransfer.files", []);
    event.preventDefault();
}, false);

dragTarget.addEventListener("dragover", function() {
    debug("On dragover:")
    event.dataTransfer.dropEffect = "copy";
    var shouldContainType = (filesToDrag.length > 0);
    checkForEventTransferType(event, "Files", shouldContainType);
    fileListShouldBe("event.dataTransfer.files", []);
    event.preventDefault();
}, false);

dragTarget.addEventListener("dragleave", function() {
    debug("On dragleave:")
    var shouldContainType = (filesToDrag.length > 0);
    checkForEventTransferType(event, "Files", shouldContainType);
    fileListShouldBe("event.dataTransfer.files", []);
}, false);

var expectedFilesOnDrop;
dragTarget.addEventListener("drop", function() {
    debug("On drop:")
    var shouldContainType = (filesToDrag.length > 0);
    checkForEventTransferType(event, "Files", shouldContainType);
    fileListShouldBe("event.dataTransfer.files", expectedFilesOnDrop);
    event.preventDefault();
}, false);

function moveMouseToCenterOfElement(element) {
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function moveMouseToOutsideOfElement(element) {
    var outsideX = element.offsetLeft + element.offsetWidth + 42;
    var outsideY = element.offsetTop + element.offsetHeight + 42;
    eventSender.mouseMoveTo(outsideX, outsideY);
}

function dragFilesOntoDragTarget(files, leave) {
    filesToDrag = files;
    eventSender.beginDragWithFiles(files);
    moveMouseToCenterOfElement(dragTarget);
    if (leave && leave === true)
        moveMouseToOutsideOfElement(dragTarget);
    eventSender.mouseUp();
}

function checkForEventTransferType(event, typeString, shouldContainType)
{
    var passedCheck;
    var message;
    if (event.dataTransfer.types && event.dataTransfer.types.indexOf(typeString) != -1) {
        passedCheck = shouldContainType;
        message = "event.dataTransfer.types contains " + typeString + ".";
    } else {
        passedCheck = !shouldContainType;
        message = "event.dataTransfer.types does not contain " + typeString + ".";
    }
    if (passedCheck)
        testPassed(message);
    else
        testFailed(message);
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

function draggingPathsShouldResultInFiles(pathsArray, filesArray, leave)
{
    expectedFilesOnDrop = filesArray;
    dragFilesOntoDragTarget(pathsArray, leave || false);
}

function testDraggingFiles(filesArray, leave)
{
    // We could make a way to parse the filename from the path, and then only need to pass
    // the path in the filesArray.
    var pathsOnly = filesArray.map(function(fileSpec) { return fileSpec['path']; });
    draggingPathsShouldResultInFiles(pathsOnly, filesArray, leave || false);
}

function runTest()
{
    debug("Dragging no files should return an empty file list (arbitrary implementation detail):");
    testDraggingFiles([]);

    debug("Drag drop a single (non-existant) file onto an element:");
    testDraggingFiles([
        { 'path': 'DRTFakeFile', 'name' : 'DRTFakeFile', 'size' : 0 }
    ]);

    debug("FIXME: File.fileSize always returns 0 for files dropped by eventSender.beginDragWithFiles from http tests:  https://bugs.webkit.org/show_bug.cgi?id=25909");

    debug("Drag files over an element, leave that element and release the mouse:");   
    testDraggingFiles([
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 }
    ], true);

    debug("Drag drop a real file onto an element:");
    testDraggingFiles([
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 }
    ]);
    
    debug("Drag drop two files onto an element:");
    testDraggingFiles([
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 },
        { 'path': 'resources/mozilla.gif', 'name' : 'mozilla.gif', 'size' : 2593 }
    ]);

    debug("Drag drop two files in reverse alphabetical order onto an element:");
    testDraggingFiles([
        { 'path': 'resources/mozilla.gif', 'name' : 'mozilla.gif', 'size' : 2593 },
        { 'path': 'resources/apple.gif', 'name' : 'apple.gif', 'size' : 1476 }
    ]);

    // Directory dragging behavior is covered by https://bugs.webkit.org/show_bug.cgi?id=25852 and https://bugs.webkit.org/show_bug.cgi?id=25879
    debug("FIXME: We should not allow element to accept drops including directories unless https://bugs.webkit.org/show_bug.cgi?id=25879 is fixed to make directory File objects useful from JavaScript.  The page is given File objects corresponding to directories, but form submission and xhr.send() will fail.");
    debug("Drag drop a directory onto an element:");
    draggingPathsShouldResultInFiles(['resources/directory-for-dragging'], []);

    // Note: The order of selection in the Finder changes the order of file paths in the pasteboard
    // thus it's important that we test different orders here as well (at least on the Mac)
    // Both drops should be refused or succeed based on how https://bugs.webkit.org/show_bug.cgi?id=25879 is resolved.  Currently we expect drops to be refused.
    debug("Drag drop a file and a directory onto an element:");
    draggingPathsShouldResultInFiles(['resources/apple.gif', 'resources/directory-for-dragging'], []);

    debug("Drag drop a directory and a file onto an element:")
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
