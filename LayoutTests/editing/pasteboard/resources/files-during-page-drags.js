description("Make sure that .files arrays are always empty for non-file drags.");

var testContainer = document.createElement('div');
// Put dragables at the top so that logging does not cause them to move off screen (where we can't drag).
document.body.insertBefore(testContainer, document.body.firstChild);

var dragTarget = document.createElement('div');
dragTarget.innerHTML = "drag target";
dragTarget.style.cssText = 'width: 100px; height: 100px; background-color: blue';
testContainer.appendChild(dragTarget);

var droppedFiles = null;
dragTarget.ondragenter = function() {
    event.dataTransfer.dropEffect = "copy";
}
dragTarget.ondragover = function() {
    event.preventDefault();
}
dragTarget.ondrop = function() {
    if (window.eventSender) {
        if (droppedFiles)
            testFailed("droppedFiles should have been cleared before making a drop");
        droppedFiles = event.dataTransfer.files;
    } else
        debug("Got drop with files: " + event.dataTransfer.files + " length: " + event.dataTransfer.files.length)
}

var image = document.createElement('img');
image.src = "resources/apple.gif";
testContainer.appendChild(image);

// If we don't add a <br> then the line height of the <a> is the same as the <img> and the click-to-drag hits undragable whitespace.
testContainer.appendChild(document.createElement('br'));

var link = document.createElement('a');
link.innerHTML = "Draggable link";
link.href = "resources/apple.gif"
testContainer.appendChild(link);

function moveMouseToCenterOfElement(element)
{
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function dragFromSourceToTarget(source, target)
{
    moveMouseToCenterOfElement(source);
    eventSender.mouseDown();
    moveMouseToCenterOfElement(target);
    eventSender.mouseUp();
}

function testDrag(source)
{
    dragFromSourceToTarget(source, dragTarget);
    if (!droppedFiles)
        testFailed("Drop of " + source.tagName + " never occured!");
    else if (droppedFiles.length == 0)
        testPassed("Drag of " + source.tagName + " resulted in empty files array.");
    else
        testFailed("Drag of " + source.tagName + " resulted in non-empty files array! (files: " + droppedFiles.length + ")");
    droppedFiles = null;
}

function runTest()
{
    testDrag(link);
    testDrag(image);
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    document.body.removeChild(testContainer);
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
