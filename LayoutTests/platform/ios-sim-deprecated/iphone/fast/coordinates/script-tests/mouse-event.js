description("This tests mouse event coordinates unscaled.");

var box = document.getElementById('box');
box.onmousedown = function(event) { debug('mousedown'); verifyCoordinates(event); }
box.onmouseup = function(event) { debug('mouseup'); verifyCoordinates(event); }
box.onclick = function(event) { debug('click'); verifyCoordinates(event); endTest(); }

function sendMouseSequence() {
    if (!window.eventSender) {
         debug("This test requires DumpRenderTree to send mouse events.");
         return;
    }

    setExpectedValues(125, 175, 125, 175);
    eventSender.mouseMoveTo(125, 175);
    eventSender.mouseDown();
    setExpectedValues(175, 125, 175, 125);
    eventSender.mouseMoveTo(175, 125);
    eventSender.mouseUp();
}

sendMouseSequence();

var successfullyParsed = true;
