var targetsDiv = document.createElement("div");
targetsDiv.id = "targetsDiv";

var div1 = document.createElement("div");
div1.id = "targetA";
div1.style.width = "100px";
div1.style.height = "100px";
div1.style.backgroundColor = "blue";

var div2 = document.createElement("div");
div2.id = "targetB";
div2.style.width = "100px";
div2.style.height = "100px";
div2.style.backgroundColor = "green";

document.body.insertBefore(targetsDiv, document.getElementById('console'));
targetsDiv.appendChild(div1);
targetsDiv.appendChild(document.createElement('br'));
targetsDiv.appendChild(div2);

function declareTouchHandler(div_id, expectedTargetTouches)
{
    return function ()
    {
        // Do not use the parameters (div_id, expectedTargetTouches) in shouldBe.. calls, as
        // the order of event dispatch is not deterministic across executions and reordering
        // ofthe PASS debug output details would cause the test to be unreliable.
        shouldBe('event.touches.length', '3');
        if (event.targetTouches.length != expectedTargetTouches)
            testFailed('Wrong targetTouch length: ' + event.targetTouches.length + ' vs ' + expectedTargetTouches);
        for (var i = 0; i < event.targetTouches.length; i++)
        {
            if (event.targetTouches[i].target.id != div_id)
                testFailed('Incorrect targetTouch ID: ' + event.targetTouches[i].target.id + ' vs ' + div_id);
        }
        shouldBe('event.changedTouches.length', '3');
    }
}

var endCount = 0;
function touchEndHandler()
{
    shouldBeEqualToString('event.type', 'touchend');

    shouldBe('event.touches.length', '0');
    shouldBe('event.targetTouches.length', '0');
    shouldBe('event.changedTouches.length', '3');

    if (++endCount == 2)
    {
        successfullyParsed = true;
        layoutTestController.notifyDone();
        isSuccessfullyParsed();
    }
}

div1.addEventListener("touchstart", declareTouchHandler('targetA', '2'), false);
div1.addEventListener("touchmove", declareTouchHandler('targetA', '2'), false);
div1.addEventListener("touchend", touchEndHandler, false);

div2.addEventListener("touchstart", declareTouchHandler('targetB', '1'), false);
div2.addEventListener("touchmove", declareTouchHandler('targetB', '1'), false);
div2.addEventListener("touchend", touchEndHandler, false);

description("Tests that the an event is sent for every touch listener, and target touches contains all the points for that target");

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
}

if (window.eventSender) {
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(50, 150);
    eventSender.addTouchPoint(50, 250);
    eventSender.addTouchPoint(50, 150);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, 200, 150);
    eventSender.updateTouchPoint(1, 300, 250);
    eventSender.updateTouchPoint(2, 400, 150);
    eventSender.touchMove();

    eventSender.releaseTouchPoint(0);
    eventSender.releaseTouchPoint(1);
    eventSender.releaseTouchPoint(2);
    eventSender.touchEnd();
} else
    debug('This test requires DRT.');

