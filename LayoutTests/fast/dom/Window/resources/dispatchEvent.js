description("Test window.dispatchEvent().");

// Test that non-events throw
var event = {};
shouldThrow("window.dispatchEvent(event)");

// Test that non-initialized events throw
event = document.createEvent("Event");
shouldThrow("window.dispatchEvent(event)");

// Test basic dispatch
var myEventDispatched = false;
var target;
var currentTarget;
window.addEventListener("myEvent", function(evt) {
    myEventDispatched = true;
    target = evt.target;
    currentTarget = evt.currentTarget;
}, false);
event = document.createEvent("Event");
event.initEvent("myEvent", false, false);
window.dispatchEvent(event);
shouldBeTrue("myEventDispatched");
shouldBe("target", "window");
shouldBe("currentTarget", "window");

// Test that both useCapture and non-useCapture listeners are dispatched to
var useCaptureDispatched = false;
window.addEventListener("myEvent", function(evt) {
    useCaptureDispatched = true;
}, true);
var nonUseCaptureDispatched = false;
window.addEventListener("myEvent", function(evt) {
    nonUseCaptureDispatched = true;
}, false);
event = document.createEvent("Event");
event.initEvent("myEvent", false, false);
window.dispatchEvent(event);
shouldBeTrue("useCaptureDispatched");
shouldBeTrue("nonUseCaptureDispatched");

var successfullyParsed = true;
