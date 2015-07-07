var iframe = document.createElement("iframe");
iframe.style.border = '1px solid black';
iframe.style.position = 'absolute';
iframe.style.left = '9px';
iframe.style.top = '9px'; // with 1px border, puts the iframe at 10,10

document.body.insertBefore(iframe, document.body.firstChild);

var iframeDoc = iframe.contentDocument;
var div = iframeDoc.createElement("div");
div.id = "touchtarget";
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

document.getElementById('console').style.marginTop = '170px';

var lastEvent = null;
var touchEventsReceived = 0;
var EXPECTED_TOUCH_EVENTS_TOTAL = 2;

function touchEventCallback(event) {
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

function mouseEventCallback(event)
{
  lastEvent = event;
  debug('Mouse event: ' + event.type);
  shouldBe("lastEvent.pageX", "12");
  shouldBe("lastEvent.pageY", "15");

  shouldBe("lastEvent.screenX", "22");
  shouldBe("lastEvent.screenY", "25");

  shouldBe("lastEvent.clientX", "12");
  shouldBe("lastEvent.clientY", "15");
}

div.addEventListener("mousedown", mouseEventCallback, false);
div.addEventListener("touchstart", touchEventCallback, false);
div.addEventListener("touchmove", touchEventCallback, false);
div.addEventListener("touchend", touchEventCallback, false);
iframeDoc.body.style.margin = '0';
iframeDoc.body.appendChild(div);

function verifyTouchEvent(type, totalTouchCount, changedTouchCount, targetTouchCount)
{
    shouldBeEqualToString("lastEvent.type", type);
    shouldBe("lastEvent.touches.length", totalTouchCount.toString());
    shouldBe("lastEvent.changedTouches.length", changedTouchCount.toString());
    shouldBe("lastEvent.targetTouches.length", targetTouchCount.toString());
}

function verifyTouch(which) {
    switch (which) {
        case 0:
           verifyTouchEvent("touchstart", 1, 1, 1);
           shouldBeEqualToString("lastEvent.touches[0].target.id", "touchtarget");

           shouldBe("lastEvent.touches[0].pageX", '12');
           shouldBe("lastEvent.touches[0].pageY", '15');

           shouldBe("lastEvent.touches[0].clientX", '12');
           shouldBe("lastEvent.touches[0].clientY", '15');

           shouldBe("lastEvent.touches[0].screenX", '22');
           shouldBe("lastEvent.touches[0].screenY", '25');

           shouldBe("lastEvent.pageX", '12');
           shouldBe("lastEvent.pageY", '15');
        break;
        case 1:
            verifyTouchEvent("touchend", 0, 1, 0);
        break;

        default: testFailed("Wrong number of touch events! (" + which + ")");
    }
}

function sendTouchSequence()
{
    eventSender.mouseMoveTo(22, 25);
    eventSender.mouseDown();
    eventSender.mouseUp();

    eventSender.addTouchPoint(22, 25);
    eventSender.touchStart();

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


