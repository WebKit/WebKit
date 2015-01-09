description("This tests mouse event coordinates at scale 2 and scrolled slightly.");

var box = document.getElementById('box');
box.onmousedown = function(event) { debug('mousedown'); verifyCoordinates(event); }
box.onmouseup = function(event) { debug('mouseup'); verifyCoordinates(event); }
box.onclick = function(event) { debug('click'); verifyCoordinates(event); endTest(); }

const scale = 2;
const panX = 10;
const panY = 10;

function sendMouseSequence() {
    if (!window.eventSender) {
         debug("This test requires DumpRenderTree to send mouse events.");
         return;
    }

    setExpectedValues(125+panX, 175+panY, 125, 175);
    eventSender.mouseMoveTo(125*scale, 175*scale);
    eventSender.mouseDown();
    setExpectedValues(175+panX, 125+panY, 175, 125);
    eventSender.mouseMoveTo(175*scale, 125*scale);
    eventSender.mouseUp();
}

setInitialScaleAndPanBy(scale, panX, panY);
sendMouseSequence();

var successfullyParsed = true;
