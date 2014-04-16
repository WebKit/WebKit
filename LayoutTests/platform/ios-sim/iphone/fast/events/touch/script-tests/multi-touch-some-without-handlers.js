var div = document.createElement("div");
div.id = "touchtarget";
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

var lastEvent = null;
var touchEventsReceived = 0;
var EXPECTED_TOUCH_EVENTS_TOTAL = 3;

function touchEventCallback() {
    if (window.eventSender) {
        lastEvent = event;
        verifyTouch(touchEventsReceived++);
    } else {
        debug(event.type);
    }

    if (window.testRunner && touchEventsReceived == EXPECTED_TOUCH_EVENTS_TOTAL) {
        // If we've got here, we can safely say we were successfully parsed :) We need to
        // call the isSucccessfullyParsed function to output the correct TEST COMPLETE
        // footer message.
        successfullyParsed = true;
        isSuccessfullyParsed();
        testRunner.notifyDone();
    }
}

div.addEventListener("touchstart", touchEventCallback, false);
div.addEventListener("touchmove", touchEventCallback, false);
div.addEventListener("touchend", touchEventCallback, false);
document.body.insertBefore(div, document.body.firstChild);

function verifyTouchEvent(type, totalTouchCount, changedTouchCount, targetTouchCount)
{
    shouldBeEqualToString("lastEvent.type", type);
    shouldBe("lastEvent.touches.length", totalTouchCount.toString());
    shouldBe("lastEvent.changedTouches.length", changedTouchCount.toString());
    shouldBe("lastEvent.targetTouches.length", targetTouchCount.toString());
}

function verifyTouchPoint(list, point, x, y, id)
{
    shouldBe("lastEvent." + list + "[" + point + "].pageX", x.toString());
    shouldBe("lastEvent." + list + "[" + point + "].pageY", y.toString());
    shouldBe("lastEvent." + list + "[" + point + "].clientX", x.toString());
    shouldBe("lastEvent." + list + "[" + point + "].clientY", y.toString());
    shouldBe("lastEvent." + list + "[" + point + "].identifier", id.toString());
}

function verifyTouch(which) {
    switch (which) {
        case 0:
            verifyTouchEvent("touchstart", 2, 1, 1);
            verifyTouchPoint("touches", 0, 10, 10, 0);
            verifyTouchPoint("touches", 1, 200, 200, 1);
            verifyTouchPoint("changedTouches", 0, 10, 10, 0);
            verifyTouchPoint("targetTouches", 0, 10, 10, 0);
            break;

        case 1:
            verifyTouchEvent("touchmove", 2, 1, 1);
            verifyTouchPoint("touches", 0, 20, 30, 0);
            verifyTouchPoint("touches", 1, 200, 200, 1);
            verifyTouchPoint("changedTouches", 0, 20, 30, 0);
            break;

        // At this point touch 2 moved but no event was triggered
        // on the target, since no touch in the target changed.

        case 2:
            verifyTouchEvent("touchend", 1, 1, 0);
            verifyTouchPoint("touches", 0, 150, 150, 1);
            verifyTouchPoint("changedTouches", 0, 20, 30, 0);
            break;

        default: testFailed("Wrong number of touch events! (" + which + ")");
    }
}

function multiTouchSequence()
{
    debug("multi touch sequence");

    debug("");
    debug("Two touchpoints, 1 in the target, 1 on the body without a target");
    eventSender.addTouchPoint(10, 10);
    eventSender.addTouchPoint(200, 200)
    eventSender.touchStart(); // Begin, Begin.

    debug("");
    debug("First touchpoint moved");
    eventSender.markAllTouchesAsStationary();
    eventSender.updateTouchPoint(0, 20, 30);
    eventSender.touchMove(); // Moved, Stationary.

    debug("");
    debug("Second touchpoint moved");
    eventSender.markAllTouchesAsStationary();
    eventSender.updateTouchPoint(1, 150, 150);
    eventSender.touchMove(); // Stationary, Moved.

    debug("");
    debug("First touchpoint is released");
    eventSender.markAllTouchesAsStationary();
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd(); // Ended, Stationary.
}

if (window.eventSender) {
    description("This tests multi touch event support where one of the touches has no touch event handler.");

    lastEvent = null;
    eventSender.clearTouchPoints();
    multiTouchSequence();
} else {
    debug("This test requires DumpRenderTree.")
}

var successfullyParsed = true;
