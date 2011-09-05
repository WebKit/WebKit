description("This tests that page scaling does not affect mouse event pageX and pageY coordinates.");

var div = document.createElement("div");
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

var eventLog = "";

function appendEventLog() {
    var msg = event.type + "(" + event.pageX + ", " + event.pageY + ")";

    if (window.eventSender) {
        eventLog += msg;
    } else {
        debug(msg);
    }
}

function clearEventLog() {
    eventLog = "";
}

div.addEventListener("click", appendEventLog, false);
document.body.insertBefore(div, document.body.firstChild);

function sendEvents(button) {
    if (!window.eventSender) {
        debug("This test requires DumpRenderTree.  Click on the blue rect with the left mouse button to log the mouse coordinates.")
        return;
    }
    eventSender.mouseDown(button);
    eventSender.mouseUp(button);
}

function testEvents(button, expectedString) {
    sendEvents(button);
    shouldBeEqualToString("eventLog", expectedString);
    clearEventLog();
}

if (window.eventSender) {
    eventSender.mouseMoveTo(10, 10);
    testEvents(0, "click(10, 10)");

    eventSender.scalePageBy(0.5, 0, 0);

    // We are clicking in the same position on screen, but we have scaled the page out by 50%,
    // we therefore expect the page-relative coordinates of the mouse event (pageX, pageY)
    // to be doubled.
    testEvents(0, "click(20, 20)");
}

var successfullyParsed = true;
