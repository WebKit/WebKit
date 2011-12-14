var div = document.createElement("div");
div.id = "touchtarget";
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

var lastEvent = null;
var touchEventsReceived = 0;
var EXPECTED_TOUCH_EVENTS_TOTAL = 5;

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
           verifyTouchEvent("touchstart", 1, 1, 1);
           shouldBe("lastEvent.shiftKey", "false");
           shouldBeEqualToString("lastEvent.touches[0].target.id", "touchtarget");
           verifyTouchPoint("touches", 0, 10, 10, 0);
           verifyTouchPoint("changedTouches", 0, 10, 10, 0);
           verifyTouchPoint("targetTouches", 0, 10, 10, 0);
        break;
        case 1:
           verifyTouchEvent("touchmove", 1, 1, 1);
           verifyTouchPoint("touches", 0, 50, 50, 0);
           shouldBe("lastEvent.shiftKey", "true");
           shouldBe("lastEvent.altKey", "true");
           shouldBe("lastEvent.ctrlKey", "false");
           shouldBe("lastEvent.metaKey", "false");
        break;
        case 2:
            verifyTouchEvent("touchend", 0, 1, 0);
            verifyTouchPoint("changedTouches", 0, 50, 50, 0);
            shouldBe("lastEvent.shiftKey", "false");
            shouldBe("lastEvent.altKey", "false");
        break;
        case 3:
            verifyTouchEvent("touchstart", 1, 1, 1);
            shouldBeEqualToString("lastEvent.targetTouches[0].target.tagName", "DIV");
        break;
        case 4:
            verifyTouchEvent("touchmove", 1, 1, 1);
            shouldBeEqualToString("lastEvent.touches[0].target.tagName", "DIV");
        break;

        default: testFailed("Wrong number of touch events! (" + which + ")");
    }
}

function singleTouchSequence()
{
    eventSender.addTouchPoint(10, 10);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, 50, 50);
    eventSender.setTouchModifier("shift", true);
    eventSender.setTouchModifier("alt", true);
    eventSender.touchMove();

    eventSender.setTouchModifier("shift", false);
    eventSender.setTouchModifier("alt", false);

    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
}

function touchTargets()
{
    eventSender.addTouchPoint(20, 20);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, 1000, 1000);
    eventSender.touchMove();
}

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

if (window.eventSender) {
    description("This tests basic single touch event support.");

    lastEvent = null;
    eventSender.clearTouchPoints();
    singleTouchSequence();

    lastEvent = null;
    eventSender.clearTouchPoints();
    touchTargets();

} else {
    debug("This test requires DumpRenderTree.  Tap on the blue rect to log.")
}


