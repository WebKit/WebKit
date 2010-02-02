description = "This tests basic touch event support.";

var div = document.createElement("div");
div.id = "touchtarget";
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

var lastEvent = null;

function appendEventLog() {
    if (window.eventSender) {
        lastEvent = event;
    } else {
        debug(event.type);
    }
}

div.addEventListener("touchstart", appendEventLog, false);
div.addEventListener("touchmove", appendEventLog, false);
div.addEventListener("touchend", appendEventLog, false);
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

function singleTouchSequence()
{
    debug("single touch sequence");

    eventSender.addTouchPoint(10, 10);
    eventSender.touchStart();

    verifyTouchEvent("touchstart", 1, 1, 1);
    shouldBe("lastEvent.shiftKey", "false");
    shouldBeEqualToString("lastEvent.touches[0].target.id", "touchtarget");
    verifyTouchPoint("touches", 0, 10, 10, 0);
    verifyTouchPoint("changedTouches", 0, 10, 10, 0);
    verifyTouchPoint("targetTouches", 0, 10, 10, 0);

    eventSender.updateTouchPoint(0, 20, 15);
    eventSender.setTouchModifier("shift", true);
    eventSender.setTouchModifier("alt", true);
    eventSender.touchMove();

    verifyTouchEvent("touchmove", 1, 1, 1);
    verifyTouchPoint("touches", 0, 20, 15, 0);
    shouldBe("lastEvent.shiftKey", "true");
    shouldBe("lastEvent.altKey", "true");
    shouldBe("lastEvent.ctrlKey", "false");
    shouldBe("lastEvent.metaKey", "false");

    eventSender.setTouchModifier("shift", false);
    eventSender.setTouchModifier("alt", false);

    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();

    verifyTouchEvent("touchend", 0, 1, 0);
    verifyTouchPoint("changedTouches", 0, 20, 15, 0);
    shouldBe("lastEvent.shiftKey", "false");
    shouldBe("lastEvent.altKey", "false");
}

function multiTouchSequence()
{
    debug("multi touch sequence");

    debug("Two touchpoints pressed");
    eventSender.addTouchPoint(10, 10);
    eventSender.addTouchPoint(20, 30);
    eventSender.touchStart();
    verifyTouchEvent("touchstart", 2, 2, 2);
    verifyTouchPoint("touches", 0, 10, 10, 0);
    verifyTouchPoint("touches", 1, 20, 30, 1);
    verifyTouchPoint("changedTouches", 0, 10, 10, 0);
    verifyTouchPoint("changedTouches", 1, 20, 30, 1);
    verifyTouchPoint("targetTouches", 0, 10, 10, 0);
    verifyTouchPoint("targetTouches", 1, 20, 30, 1);

    debug("First touchpoint moved");
    eventSender.updateTouchPoint(0, 15, 15);
    eventSender.touchMove();
    verifyTouchEvent("touchmove", 2, 1, 2);
    verifyTouchPoint("touches", 0, 15, 15, 0);
    verifyTouchPoint("changedTouches", 0, 15, 15, 0);
    verifyTouchPoint("touches", 1, 20, 30, 1);

    debug("First touchpoint is released");
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
    verifyTouchEvent("touchend", 1, 1, 1);
    verifyTouchPoint("touches", 0, 20, 30, 1);
    verifyTouchPoint("changedTouches", 0, 15, 15, 0);
    verifyTouchPoint("targetTouches", 0, 20, 30, 1);

    debug("Last remaining touchpoint is released");
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
    verifyTouchEvent("touchend", 0, 1, 0);
    verifyTouchPoint("changedTouches", 0, 20, 30, 1);
}

function touchTargets()
{
    debug("verify touch targets");

    eventSender.addTouchPoint(10, 10);
    eventSender.touchStart();

    verifyTouchEvent("touchstart", 1, 1, 1);
    shouldBeEqualToString("lastEvent.targetTouches[0].target.tagName", "DIV");

    eventSender.updateTouchPoint(0, 1000, 1000);
    eventSender.touchMove();

    verifyTouchEvent("touchmove", 1, 1, 0);
    shouldBeEqualToString("lastEvent.touches[0].target.tagName", "HTML");
}

if (window.eventSender) {
    debug(description);

    lastEvent = null;
    eventSender.clearTouchPoints();
    singleTouchSequence();

    lastEvent = null;
    eventSender.clearTouchPoints();
    multiTouchSequence();

    lastEvent = null;
    eventSender.clearTouchPoints();
    touchTargets();

} else {
    debug("This test requires DumpRenderTree.  Tap on the blue rect to log.")
}

var successfullyParsed = true;
