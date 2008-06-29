description("This tests what mouse events we send.");

var div = document.createElement("div");
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

function logEvent() {
    debug(event.type + "(" + event.button + ")");
}

div.addEventListener("click", logEvent, false);
div.addEventListener("dblclick", logEvent, false);
div.addEventListener("mousedown", logEvent, false);
div.addEventListener("mouseup", logEvent, false);
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

debug("Left Mouse Button");
sendEvents(0);

debug("Middle Mouse Button");
sendEvents(1);

debug("Right Mouse Button");
sendEvents(2);

debug("4th Mouse Button");
sendEvents(3);

var successfullyParsed = true;
