var div = document.createElement("div");
div.id = "touchtarget";
div.style.width = "100px";
div.style.height = "100px";
div.style.color = "blue";
div.innerText = "Touch Me!";
div.style.webkitTapHighlightColor = "rgba(11, 22, 33, 0)";

function onTouchStart()
{
    debug("Event: touch start");
    shouldBeEqualToString("event.targetTouches[0].target.tagName", "DIV");
    shouldBeEqualToString("event.targetTouches[0].target.id", "touchtarget");
    shouldBeEqualToString("div.ownerDocument.defaultView.getComputedStyle(div, null).getPropertyValue('-webkit-tap-highlight-color')", "rgba(11, 22, 33, 0)");
    div.style.webkitTapHighlightColor = "";  // Clear the customized tap highlight color
}

function onTouchEnd()
{
    debug("Event: touch end");
    // Check the default value.
    shouldBeFalse("div.ownerDocument.defaultView.getComputedStyle(div, null).getPropertyValue('-webkit-tap-highlight-color') == 'rgba(11, 22, 33, 0)'")
    shouldBeNonNull("div.ownerDocument.defaultView.getComputedStyle(div, null).getPropertyValue('-webkit-tap-highlight-color')");
    // Notify the test done.
    isSuccessfullyParsed();
    testRunner.notifyDone();
}

div.addEventListener("touchstart", onTouchStart, false);
div.addEventListener("touchend", onTouchEnd, false);
document.body.insertBefore(div, document.body.firstChild);

function touchTargets()
{
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(20, 20);
    eventSender.touchStart();
    eventSender.updateTouchPoint(0, 50, 50);
    eventSender.touchMove();
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
}

if (window.testRunner)
    testRunner.waitUntilDone();

if (window.eventSender && eventSender.clearTouchPoints) {
    description("Check tap highlight color in touch event");

    touchTargets();
} else {
    debug("This test requires DumpRenderTree && WebKit built with ENABLE(TOUCH_EVENT).")
}
