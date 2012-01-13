var counter = 0;
var tests = 8;
var expected = "";
var logEvent = false;
var rectElement = document.getElementById("target");
var useElement = document.getElementById("test");
description("Test attaching event listeners on SVG use elements in different ways: ");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function eventHandler(evt)
{
    if (logEvent) {
        if (expected == evt.type)
            debug("Test " + counter + " / " + tests + " PASSED");
        else
            debug("Test " + counter + " / " + tests + " FAILED (expected: '" + expected + "' actual: '" + evt.type + "')");
    }

    setTimeout(counter < tests ? driveTests : finishTest, 0);
}

function finishTest()
{
    successfullyParsed = true;

    useElement.instanceRoot.correspondingElement.setAttribute("fill", "green");
    shouldBeTrue("successfullyParsed");
    debug('<br /><span class="pass">TEST COMPLETE</span>');

    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

function recordMouseEvent(type)
{
    expected = type;
    logEvent = true;

    ++counter;
    fireDelayedEvent();
}

function sendMouseEvent(type, increment)
{
    expected = type;
    logEvent = false;
    fireDelayedEvent();
}

function fireDelayedEvent()
{
    if (expected == "mouseover")
        eventSender.mouseMoveTo(50, 50);
    else if (expected == "mouseout")
        eventSender.mouseMoveTo(115, 55);
    else if (expected == "mouseup") {
        eventSender.mouseMoveTo(50, 50);
        eventSender.mouseDown();
        eventSender.mouseUp();
    } else if (expected == "mousedown") {
        eventSender.mouseMoveTo(50, 50);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function testOne()
{
    // Install event listener on correspondingElement via setAttribute
    useElement.instanceRoot.correspondingElement.setAttribute("onmouseover", "eventHandler(evt)");
    recordMouseEvent("mouseover");
}

function testTwo()
{
    // Install event listener on correspondingElement via onmouseout JS magic
    useElement.instanceRoot.correspondingElement.onmouseout = eventHandler;
    recordMouseEvent("mouseout");
}

function testThree()
{
    // Clean event listeners on different ways
    useElement.instanceRoot.correspondingElement.removeAttribute("onmouseover");
    useElement.instanceRoot.correspondingElement.onmouseout = 0;

    // Verify they really got removed
    sendMouseEvent("mouseover");
    sendMouseEvent("mouseout");

    // Verify the original listener still works
    recordMouseEvent("mousedown");
}

function testFour()
{
    useElement.instanceRoot.correspondingElement.removeAttribute("onmousedown");

    // Install event listener on the referenced element, without using the SVGElementInstance interface
    rectElement.setAttribute("onmouseup", "eventHandler(evt)");
    recordMouseEvent("mouseup");
}

function testFive()
{
    rectElement.onmouseout = eventHandler;
    recordMouseEvent("mouseout");
}

function testSix()
{
    useElement.instanceRoot.correspondingElement.onmouseout = null;
    sendMouseEvent("mouseout");

    useElement.instanceRoot.correspondingElement.removeAttribute('onmouseup');
    sendMouseEvent("mouseup");

    useElement.instanceRoot.correspondingElement.onmouseup = eventHandler;
    recordMouseEvent("mouseup");
}

function testSeven()
{
    useElement.instanceRoot.addEventListener("mouseout", eventHandler, false);
    recordMouseEvent("mouseout");
}

function testEight()
{
    useElement.instanceRoot.correspondingElement.addEventListener("mouseover", eventHandler, false);
    recordMouseEvent("mouseover");
}

function testNine()
{
    rectElement.addEventListener("mousedown", eventHandler, false);
    recordMouseEvent("mousedown");
}

function driveTests()
{
    switch (counter) {
    case 0:
        testOne();
        break;
    case 1:
        testTwo();
        break;
    case 2:
        testThree();
        break;
    case 3:
        testFour();
        break;
    case 4:
        testFive();
        break;
    case 5:
        testSix();
        break;
    case 6:
        testSeven();
        break;
    case 7:
        testEight();
        break;
    }
}

// Start tests
if (window.eventSender) {
    document.documentElement.offsetLeft;
    eventSender.mouseMoveTo(115, 55);
    driveTests();
} else
    alert("This test must be run via DRT!");
