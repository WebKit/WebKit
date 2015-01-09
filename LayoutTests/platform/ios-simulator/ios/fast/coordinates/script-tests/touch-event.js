description("This tests touch event coordinates unscaled.");

var box = document.getElementById('box');
box.ontouchstart = function(event) { debug('touchstart'); verifyCoordinates(event.touches[0]); }
box.ontouchmove = function(event) { debug('touchmove'); verifyCoordinates(event.touches[0]); }
box.ontouchend = function(event) { debug('touchend - no touch to check'); endTest(); }

const FIRST_FINGER = 0;
function sendTouchSequence() {
    if (!window.eventSender) {
        debug("This test requires DumpRenderTree to send touch events.");
        return;
    }

    setExpectedValues(125, 175, 125, 175);
    eventSender.addTouchPoint(125, 175);
    eventSender.touchStart();

    setExpectedValues(175, 125, 175, 125);
    eventSender.updateTouchPoint(FIRST_FINGER, 175, 125);
    eventSender.touchMove();

    eventSender.releaseTouchPoint(FIRST_FINGER);
    eventSender.touchEnd(); // The test will end with this release.
}

sendTouchSequence();

var successfullyParsed = true;
