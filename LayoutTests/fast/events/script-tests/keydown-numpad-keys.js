description("This tests keyboard events with KeyLocationCode argument.");

var lastKeyboardEvent;

function recordKeyEvent(ev) {
    ev = ev || event;
    ev.keyCode = (ev.which || ev.keyCode);
    if (window.eventSender) {
        lastKeyboardEvent = ev;
    } else {
        debug('Type=' + ev.type + ',' +
              'keyCode=' + ev.keyCode + ',' +
              'ctrlKey=' + ev.ctrlKey + ',' +
              'shiftKey=' + ev.shiftKey + ',' +
              'altKey=' + ev.altKey + ',' +
              'metaKey=' + ev.metaKey + ',' +
              'location=' + ev.keyLocation);
    }
}

function testKeyEventWithLocation(evString, evLocation, expectedKeyCode) {
    eventSender.keyDown(evString, [], evLocation);
    shouldBe("lastKeyboardEvent.type", '"keydown"');
    shouldEvaluateTo("lastKeyboardEvent.keyCode", expectedKeyCode);
    shouldEvaluateTo("lastKeyboardEvent.keyLocation", evLocation);
}

var textarea = document.createElement("textarea");
textarea.addEventListener("keydown", recordKeyEvent, false);
document.body.insertBefore(textarea, document.body.firstChild);
textarea.focus();

if (window.eventSender) {
    // location=0 indicates that we send events as standard keys.
    testKeyEventWithLocation("pageUp", 0, 33);
    testKeyEventWithLocation("pageDown", 0, 34);
    testKeyEventWithLocation("home", 0, 36);
    testKeyEventWithLocation("end", 0, 35);
    testKeyEventWithLocation("leftArrow", 0, 37);
    testKeyEventWithLocation("rightArrow", 0, 39);
    testKeyEventWithLocation("upArrow", 0, 38);
    testKeyEventWithLocation("downArrow", 0, 40);
    testKeyEventWithLocation("insert", 0, 45);
    testKeyEventWithLocation("delete", 0, 46);

    // location=3 indicates that we send events as numeric-pad keys.
    testKeyEventWithLocation("pageUp", 3, 33);
    testKeyEventWithLocation("pageDown", 3, 34);
    testKeyEventWithLocation("home", 3, 36);
    testKeyEventWithLocation("end", 3, 35);
    testKeyEventWithLocation("leftArrow", 3, 37);
    testKeyEventWithLocation("rightArrow", 3, 39);
    testKeyEventWithLocation("upArrow", 3, 38);
    testKeyEventWithLocation("downArrow", 3, 40);
    testKeyEventWithLocation("insert", 3, 45);
    testKeyEventWithLocation("delete", 3, 46);
} else {
    debug("This test requires DumpRenderTree.  To manually test, 1) focus on the textarea above and push numpad keys without locking NumLock and 2) see if the location= value is 3 (DOM_KEY_LOCATION_NUMPAD specified in DOM level 3).");
}
