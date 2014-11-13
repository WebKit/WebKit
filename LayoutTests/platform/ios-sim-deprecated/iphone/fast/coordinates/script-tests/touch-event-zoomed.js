description("This tests touch event coordinates at scale 2 and scrolled slightly.");

var box = document.getElementById('box');
box.ontouchstart = function(event) { debug('touchstart'); verifyCoordinates(event.touches[0]); }
box.ontouchmove = function(event) { debug('touchmove'); verifyCoordinates(event.touches[0]); }
box.ontouchend = function(event) { debug('touchend - no touch to check'); endTest(); }

const scale = 2;
const panX = 10;
const panY = 10;
const FIRST_FINGER = 0;

function sendTouchSequence() {
    if (!window.eventSender) {
        debug("This test requires DumpRenderTree to send touch events.");
        return;
    }

    setExpectedValues(125+panX, 175+panY, 125, 175);
    eventSender.addTouchPoint(125*scale, 175*scale);
    eventSender.touchStart();

    setExpectedValues(175+panX, 125+panY, 175, 125);
    eventSender.updateTouchPoint(FIRST_FINGER, 175*scale, 125*scale);
    eventSender.touchMove();

    eventSender.releaseTouchPoint(FIRST_FINGER);
    eventSender.touchEnd(); // The test will end with this release.
}

setInitialScaleAndPanBy(scale, panX, panY);
sendTouchSequence();

var successfullyParsed = true;
