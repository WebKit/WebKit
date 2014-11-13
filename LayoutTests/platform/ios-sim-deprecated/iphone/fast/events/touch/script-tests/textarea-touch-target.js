var target = document.createElement('textarea');
target.id = "target";
target.setAttribute('style', "position: absolute; top: 0; left: 0; font-size: 24px;");

document.body.insertBefore(target, document.getElementById('console'));

function touchStartHandler()
{
    shouldBeEqualToString('event.type', 'touchstart');
    shouldBeEqualToString('event.touches[0].target.id', target.id);
    shouldBeEqualToString('event.touches[0].target.nodeName', 'TEXTAREA');

    successfullyParsed = true;
    isSuccessfullyParsed();
}

target.addEventListener("touchstart", touchStartHandler, false);

description("Tests that input elements can receive touch events.");

if (window.eventSender) {
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(10, 10);
    eventSender.touchStart();
    eventSender.touchEnd();
} else
    debug('This test requires DRT.');

document.body.removeChild(target);

var successfullyParsed = true;
