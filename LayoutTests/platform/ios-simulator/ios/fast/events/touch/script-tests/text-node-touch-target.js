var targetDiv = document.createElement("div");
targetDiv.id = "targetDiv";
targetDiv.setAttribute('style', "position: absolute; top: 0; left: 0; background-color: blue; font-size: 24px;");
targetDiv.innerText = 'This is some text';

document.body.insertBefore(targetDiv, document.getElementById('console'));

function touchStartHandler()
{
    shouldBeEqualToString('event.type', 'touchstart');
    shouldBeEqualToString('event.touches[0].target.id', targetDiv.id);
    shouldBeEqualToString('event.touches[0].target.nodeName', 'DIV');

    successfullyParsed = true;
    isSuccessfullyParsed();
}

targetDiv.addEventListener("touchstart", touchStartHandler, false);

description("Tests that the event target for a touch on a text node is the parent element, not the text node.");

if (window.eventSender) {
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(10, 10);
    eventSender.touchStart();
    eventSender.touchEnd();
} else
    debug('This test requires DRT.');


var successfullyParsed = true;
