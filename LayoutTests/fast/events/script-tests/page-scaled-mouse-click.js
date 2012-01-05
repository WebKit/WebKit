description("This tests that page scaling does not affect mouse event pageX and pageY coordinates.");

var html = document.documentElement;

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

function testEvents(button, description, expectedString) {
    sendEvents(button);
    debug(description);
    shouldBeEqualToString("eventLog", expectedString);
    debug("");
    clearEventLog();
}

if (window.eventSender && window.internals) {
    eventSender.mouseMoveTo(10, 10);
    // We are clicking in the same position on screen. As we scale or transform the page,
    // we expect the pageX and pageY event coordinates to change because different
    // parts of the document are under the mouse.
    testEvents(0, "Unscaled", "click(10, 10)");

    window.internals.setPageScaleFactor(document, 0.5, 0, 0);
    testEvents(0, "setPageScale(0.5)", "click(20, 20)");
}
