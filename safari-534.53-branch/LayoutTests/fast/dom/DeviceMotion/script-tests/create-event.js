description("Tests that document.createEvent() works with DeviceMotinEvent.");

var event = document.createEvent('DeviceMotionEvent');

shouldBeTrue("typeof event == 'object'");

shouldBeTrue("'type' in event");
shouldBeTrue("'bubbles' in event");
shouldBeTrue("'cancelable' in event");
shouldBeTrue("'acceleration' in event");
shouldBeTrue("'accelerationIncludingGravity' in event");
shouldBeTrue("'rotationRate' in event");
shouldBeTrue("'interval' in event");

shouldBeTrue("typeof event.type == 'string'");
shouldBeTrue("typeof event.bubbles == 'boolean'");
shouldBeTrue("typeof event.cancelable == 'boolean'");
shouldBeTrue("typeof event.acceleration == 'object'");
shouldBeTrue("typeof event.accelerationIncludingGravity == 'object'");
shouldBeTrue("typeof event.rotationRate == 'object'");
shouldBeTrue("typeof event.interval == 'object'");

window.successfullyParsed = true;
