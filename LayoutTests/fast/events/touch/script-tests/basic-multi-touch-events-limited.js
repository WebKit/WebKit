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

    if (window.layoutTestController && touchEventsReceived == EXPECTED_TOUCH_EVENTS_TOTAL) {
        // If we've got here, we can safely say we were successfully parsed :) We need to
        // call the isSucccessfullyParsed function to output the correct TEST COMPLETE
        // footer message.
        successfullyParsed = true;
        isSuccessfullyParsed();
        layoutTestController.notifyDone();
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
    shouldBe("lastEvent.pageX", "0");
    shouldBe("lastEvent.pageY", "0");
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
            verifyTouchEvent("touchstart", 2, 2, 2);
            verifyTouchPoint("touches", 0, 10, 10, 0);
            verifyTouchPoint("touches", 1, 20, 30, 1);
            verifyTouchPoint("changedTouches", 0, 10, 10, 0);
            verifyTouchPoint("changedTouches", 1, 20, 30, 1);
            verifyTouchPoint("targetTouches", 0, 10, 10, 0);
            verifyTouchPoint("targetTouches", 1, 20, 30, 1);
        break;
        case 1:
            verifyTouchEvent("touchmove", 2, 2, 2);
            verifyTouchPoint("touches", 0, 15, 15, 0);
            verifyTouchPoint("touches", 1, 25, 35, 1);
            verifyTouchPoint("changedTouches", 0, 15, 15, 0);
            verifyTouchPoint("changedTouches", 1, 25, 35, 1);
            verifyTouchPoint("targetTouches", 0, 15, 15, 0);
            verifyTouchPoint("targetTouches", 1, 25, 35, 1);
        break;
        case 2:
            verifyTouchEvent("touchend", 0, 2, 0);
            verifyTouchPoint("changedTouches", 0, 15, 15, 0);
            verifyTouchPoint("changedTouches", 1, 25, 35, 1);
        break;

        default: testFailed("Wrong number of touch events! (" + which + ")");
    }
}

function multiTouchSequence()
{
    eventSender.addTouchPoint(10, 10);
    eventSender.addTouchPoint(20, 30);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, 15, 15);
    eventSender.updateTouchPoint(1, 25, 35);
    eventSender.touchMove();

    eventSender.releaseTouchPoint(0);
    eventSender.releaseTouchPoint(1);
    eventSender.touchEnd();
}

if (window.eventSender) {
    description("This tests basic multi touch event support. This is a limited version of test basic-multi-touch-events.html that avoids the situation where one touch point is released while another is maintained.");

    lastEvent = null;
    eventSender.clearTouchPoints();
    multiTouchSequence();
} else {
    debug("This test requires DumpRenderTree.  Tap on the blue rect to log.")
}

var successfullyParsed = true;
