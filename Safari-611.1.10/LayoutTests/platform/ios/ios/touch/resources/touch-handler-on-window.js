var targetDiv = document.createElement("div");
targetDiv.id = "targetDiv";
targetDiv.setAttribute('style', "position: absolute; top: 100px; left: 10px; width: 400px; height: 400px;");

document.body.insertBefore(targetDiv, document.getElementById('console'));

function logTouchEvent(event) {
  debug("Window received touch event " + event.type + " with target " + event.target.tagName);
}

window.addEventListener('touchstart', logTouchEvent, false);
window.addEventListener('touchmove', logTouchEvent, false);
window.addEventListener('touchend', logTouchEvent, false);
window.addEventListener('gesturestart', logTouchEvent, false);
window.addEventListener('gesturechange', logTouchEvent, false);
window.addEventListener('gestureend', logTouchEvent, false);

description("Tests that touch events are dispatched to the DOMWindow even if no other elements have touch event listeners.");

if (window.eventSender) {
    // Test tapping in a div.
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(20, 220);
    eventSender.touchStart();
    eventSender.updateTouchPoint(0, 20, 120);
    eventSender.touchMove();
    eventSender.touchEnd();

    // Test tapping in the body.
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(500, 500);
    eventSender.touchStart();
    eventSender.touchEnd();

    // Test that gesture events also make it to the window.
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(30, 130);
    eventSender.addTouchPoint(40, 140);
    eventSender.touchStart();
    eventSender.updateTouchPoint(0, 50, 150);
    eventSender.touchMove();
    eventSender.touchEnd();
} else
    debug('This test requires DRT.');

var successfullyParsed = true;
