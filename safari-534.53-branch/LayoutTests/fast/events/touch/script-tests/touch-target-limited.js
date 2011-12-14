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

function declareTouchStart()
{
    var touchStartCount = 0;
    return function touchStartHandler()
    {
        shouldBeEqualToString('event.type', 'touchstart');
        switch (touchStartCount) {
            case 0:
                shouldBeEqualToString('event.touches[0].target.id', div1.id);
                shouldBeEqualToString('event.touches[1].target.id', div2.id);
                break;
            case 1:
                shouldBeEqualToString('event.touches[0].target.id', div2.id);
                shouldBeEqualToString('event.touches[1].target.id', div1.id);
                break;
        }
        shouldBe('event.targetTouches.length', '1');

        touchStartCount++;
    }
}

var totalTouchMoveCount = 0;

function declareTouchMove(div_id)
{
    var touchMoveCount = 0;
    return function touchMoveHandler()
    {
        shouldBeEqualToString('event.type', 'touchmove');
        switch (touchMoveCount) {
            case 0:
            case 1:
                shouldBeEqualToString('event.touches[0].target.id', div1.id);
                shouldBeEqualToString('event.touches[1].target.id', div2.id);
                break;
            case 2:
                shouldBeEqualToString('event.touches[0].target.id', div2.id);
                shouldBeEqualToString('event.touches[1].target.id', div1.id);
                break;
        }
        shouldBe('event.targetTouches.length', '1');
        ++touchMoveCount;

        if (++totalTouchMoveCount == 6)
        {
            successfullyParsed = true;
            layoutTestController.notifyDone();
            isSuccessfullyParsed();
        }
    }
}

div1.addEventListener("touchstart", declareTouchStart(), false);
div1.addEventListener("touchmove", declareTouchMove(), false);

div2.addEventListener("touchstart", declareTouchStart(), false);
div2.addEventListener("touchmove", declareTouchMove(), false);

description("Tests that the target of touches match the element where the event originated, not where the touch is currently occurring. This is a limited version of test touch-target.html that avoids the situation where one touch point is released while another is maintained.");

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
}

if (window.eventSender) {
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(50, 150);
    eventSender.addTouchPoint(50, 250);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, 50, 250);
    eventSender.updateTouchPoint(1, 50, 150);
    eventSender.touchMove();

    eventSender.updateTouchPoint(0, 1000, 1000);
    eventSender.updateTouchPoint(1, 1000, 1000);
    eventSender.touchMove();

    eventSender.releaseTouchPoint(0);
    eventSender.releaseTouchPoint(1);
    eventSender.touchEnd();

    eventSender.addTouchPoint(50, 250);
    eventSender.addTouchPoint(50, 150);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, 500, 500);
    eventSender.updateTouchPoint(1, 500, 500);
    eventSender.touchMove();
} else
    debug('This test requires DRT.');

