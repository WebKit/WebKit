var div = document.createElement("div");
div.id = "touchtarget";
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

var lastEvent = null;
var touchEventsReceived = 0;
var EXPECTED_TOUCH_EVENTS_TOTAL = 4;

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
            verifyTouchEvent("touchmove", 2, 1, 2);
            verifyTouchPoint("touches", 0, 15, 15, 0);
            verifyTouchPoint("changedTouches", 0, 15, 15, 0);
            verifyTouchPoint("touches", 1, 20, 30, 1);
        break;
        case 2:
            verifyTouchEvent("touchend", 1, 1, 1);
            verifyTouchPoint("touches", 0, 20, 30, 1);
            verifyTouchPoint("changedTouches", 0, 15, 15, 0);
            verifyTouchPoint("targetTouches", 0, 20, 30, 1);
        break;
        case 3:
            verifyTouchEvent("touchend", 0, 1, 0);
            verifyTouchPoint("changedTouches", 0, 20, 30, 1);
        break;

        default: testFailed("Wrong number of touch events! (" + which + ")");
    }
}

function multiTouchSequence()
{
    debug("multi touch sequence");

    debug("Two touchpoints pressed");
    eventSender.addTouchPoint(10, 10);
    eventSender.addTouchPoint(20, 30);
    eventSender.touchStart();

    debug("First touchpoint moved");
    eventSender.updateTouchPoint(0, 15, 15);
    eventSender.touchMove();

    debug("First touchpoint is released");
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();

    debug("Last remaining touchpoint is released");
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
}

if (window.eventSender) {
    description("This tests basic multi touch event support.");

    lastEvent = null;
    eventSender.clearTouchPoints();
    multiTouchSequence();
} else {
    debug("This test requires DumpRenderTree.  Tap on the blue rect to log.")
}

var successfullyParsed = true;
