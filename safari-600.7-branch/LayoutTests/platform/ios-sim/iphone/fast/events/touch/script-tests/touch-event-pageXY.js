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
           verifyTouchEvent("touchstart", 1, 1, 1);
           shouldBeEqualToString("lastEvent.touches[0].target.id", "touchtarget");

           verifyTouchPoint("touches", 0, 8, 12, 0);
           verifyTouchPoint("changedTouches", 0, 8, 12, 0);
           verifyTouchPoint("targetTouches", 0, 8, 12, 0);

           shouldBe("lastEvent.pageX", '8');
           shouldBe("lastEvent.pageY", '12');
        break;
        case 1:
           verifyTouchEvent("touchmove", 1, 1, 1);
           verifyTouchPoint("touches", 0, 10, 15, 0);

           shouldBe("lastEvent.pageX", '10');
           shouldBe("lastEvent.pageY", '15');
        break;
        case 2:
           verifyTouchEvent("touchstart", 2, 1, 2);
           verifyTouchPoint("touches", 0, 10, 15, 0);
           verifyTouchPoint("touches", 1, 40, 45, 1);

           shouldBe("lastEvent.pageX", '25');
           shouldBe("lastEvent.pageY", '30');
        break;
        case 3:
            verifyTouchEvent("touchend", 1, 1, 1);

            shouldBe("lastEvent.pageX", '40');
            shouldBe("lastEvent.pageY", '45');
        break;

        default: testFailed("Wrong number of touch events! (" + which + ")");
    }
}

function sendTouchSequence()
{
    eventSender.addTouchPoint(8, 12);
    eventSender.touchStart();

    debug('move');
    eventSender.updateTouchPoint(0, 10, 15);
    eventSender.touchMove();

    debug('add second touch');
    eventSender.addTouchPoint(40, 45);
    eventSender.touchStart();

    debug('end');
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
}

if (window.testRunner)
    testRunner.waitUntilDone();

if (window.eventSender) {
    description("This tests pageX and pageY coordinates on touch events and touches.");

    lastEvent = null;
    sendTouchSequence();

} else {
    debug("This test requires DumpRenderTree.  Tap on the blue rect to log.")
}


