var _testFrame;
var _testFrameId; // We keep the id of the test frame to make it easier for a person to interpret the test results.

function setTestFrameById(id)
{
    _testFrameId = id;
    _testFrame = document.getElementById(id);
}

function deltaWidth()
{
    return 10; // A resonable amount to be able to detect that the frame width changed.
}

function shouldAllowFrameResize()
{
    shouldAllowFrameResizeAfterProcessingFrame(function(frame) { /* Do nothing. */});
}

function shouldDisallowFrameResize()
{
    shouldDisallowFrameResizeAfterProcessingFrame(function(frame) { /* Do nothing. */})
}

function shouldDisallowFrameResizeAfterProcessingFrame(processFrameFunction)
{
    var expectedWidth = _testFrame.width;
    processFrameFunction(_testFrame);
    resizeTestFrameBy(deltaWidth());
    checkTestFrameWidthEquals(expectedWidth);
}

function shouldAllowFrameResizeAfterProcessingFrame(processFrameFunction)
{
    var expectedWidth = _testFrame.width + deltaWidth();
    processFrameFunction(_testFrame);
    resizeTestFrameBy(deltaWidth());
    checkTestFrameWidthEquals(expectedWidth);
}

function checkTestFrameWidthEquals(expectedWidth)
{
    if (_testFrame.width === expectedWidth)
        log('PASS document.getElementById("' + _testFrameId + '").width is ' + expectedWidth);
    else
        log('FAIL document.getElementById("' + _testFrameId + '").width should be ' + expectedWidth + '. Was ' + _testFrame.width + '.');
}

function resizeTestFrameBy(deltaWidthInPixels)
{
    var borderWidth = parseInt(_testFrame.parentNode.getAttribute("border"));

    var startX = _testFrame.offsetWidth + borderWidth / 2;
    var startY = _testFrame.offsetHeight / 2;
    var endX = startX + deltaWidthInPixels;
    var endY = startY;

    eventSender.mouseMoveTo(startX, startY);
    eventSender.mouseDown();
    eventSender.leapForward(100);
    eventSender.mouseMoveTo(endX, endY);
    eventSender.mouseUp();
}

function log(message)
{
    document.getElementById("results").contentDocument.getElementById("console").appendChild(document.createTextNode(message + "\n"));
}

function description(message)
{
    document.getElementById("results").contentDocument.getElementById("description").innerText = message;
}
