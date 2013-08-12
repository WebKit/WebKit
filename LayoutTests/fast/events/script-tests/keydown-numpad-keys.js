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
              'location=' + ev.location);
    }
}

function testKeyEventWithLocation(evString, evLocation, expectedKeyCode) {
    eventSender.keyDown(evString, [], eval(evLocation));
    shouldBe("lastKeyboardEvent.type", '"keydown"');
    shouldEvaluateTo("lastKeyboardEvent.keyCode", expectedKeyCode);
    shouldEvaluateTo("lastKeyboardEvent.location", evLocation);
}

var textarea = document.createElement("textarea");
textarea.addEventListener("keydown", recordKeyEvent, false);
document.body.insertBefore(textarea, document.body.firstChild);
textarea.focus();

if (window.eventSender) {
    // location=0 indicates that we send events as standard keys.
    testKeyEventWithLocation("pageUp", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 33);
    testKeyEventWithLocation("pageDown", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 34);
    testKeyEventWithLocation("home", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 36);
    testKeyEventWithLocation("end", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 35);
    testKeyEventWithLocation("leftArrow", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 37);
    testKeyEventWithLocation("rightArrow", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 39);
    testKeyEventWithLocation("upArrow", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 38);
    testKeyEventWithLocation("downArrow", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 40);
    testKeyEventWithLocation("insert", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 45);
    testKeyEventWithLocation("delete", "KeyboardEvent.DOM_KEY_LOCATION_STANDARD", 46);

    // location=3 indicates that we send events as numeric-pad keys.
    testKeyEventWithLocation("pageUp", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 33);
    testKeyEventWithLocation("pageDown", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 34);
    testKeyEventWithLocation("home", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 36);
    testKeyEventWithLocation("end", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 35);
    testKeyEventWithLocation("leftArrow", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 37);
    testKeyEventWithLocation("rightArrow", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 39);
    testKeyEventWithLocation("upArrow", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 38);
    testKeyEventWithLocation("downArrow", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 40);
    testKeyEventWithLocation("insert", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 45);
    testKeyEventWithLocation("delete", "KeyboardEvent.DOM_KEY_LOCATION_NUMPAD", 46);
} else {
    debug("This test requires DumpRenderTree.  To manually test, 1) focus on the textarea above and push numpad keys without locking NumLock and 2) see if the location= value is KeyboardEvent.DOM_KEY_LOCATION_NUMPAD (specified in DOM level 3).");
}
