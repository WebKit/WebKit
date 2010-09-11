description("This tests what mouse events we send.");

var div = document.createElement("div");
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

var eventLog = "";

function appendEventLog() {
    if (window.eventSender) {
        eventLog += event.type + "(" + event.button + ") ";
    } else {
        debug(event.type + "(" + event.button + ")");
    }
}

function clearEventLog() {
    eventLog = "";
}

div.addEventListener("click", appendEventLog, false);
div.addEventListener("dblclick", appendEventLog, false);
div.addEventListener("mousedown", appendEventLog, false);
div.addEventListener("mouseup", appendEventLog, false);
document.body.insertBefore(div, document.body.firstChild);

if (window.eventSender)
    eventSender.mouseMoveTo(10, 10);

function sendEvents(button) {
    if (!window.eventSender) {
        debug("This test requires DumpRenderTree.  Click on the blue rect with different mouse buttons to log.")
        return;
    }
    eventSender.mouseDown(button);
    eventSender.mouseUp(button);
    eventSender.mouseDown(button);
    eventSender.mouseUp(button);
    // could test dragging here too
}

function testEvents(description, button, expectedString) {
    debug(description);
    sendEvents(button);
    shouldBeEqualToString("eventLog", expectedString);
    clearEventLog();
}

if (window.eventSender) {
    testEvents("Left Mouse Button", 0, "mousedown(0) mouseup(0) click(0) mousedown(0) mouseup(0) click(0) dblclick(0) ");
    testEvents("Middle Mouse Button", 1, "mousedown(1) mouseup(1) mousedown(1) mouseup(1) ");
    testEvents("Right Mouse Button", 2, "mousedown(2) mouseup(2) mousedown(2) mouseup(2) ");
    testEvents("4th Mouse Button", 3, "mousedown(1) mouseup(1) mousedown(1) mouseup(1) ");
}

var successfullyParsed = true;
