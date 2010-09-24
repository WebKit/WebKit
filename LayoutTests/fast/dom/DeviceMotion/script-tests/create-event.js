description("Tests that document.createEvent() works with DeviceMotinEvent.");

var event = document.createEvent('DeviceMotionEvent');

shouldBeTrue("typeof event == 'object'");

shouldBeTrue("'type' in event");
shouldBeTrue("'bubbles' in event");
shouldBeTrue("'cancelable' in event");
shouldBeTrue("'xAcceleration' in event");
shouldBeTrue("'yAcceleration' in event");
shouldBeTrue("'zAcceleration' in event");
shouldBeTrue("'xRotationRate' in event");
shouldBeTrue("'yRotationRate' in event");
shouldBeTrue("'zRotationRate' in event");
shouldBeTrue("'interval' in event");

shouldBeTrue("typeof event.type == 'string'");
shouldBeTrue("typeof event.bubbles == 'boolean'");
shouldBeTrue("typeof event.cancelable == 'boolean'");
shouldBeTrue("typeof event.xAcceleration == 'object'");
shouldBeTrue("typeof event.yAcceleration == 'object'");
shouldBeTrue("typeof event.zAcceleration == 'object'");
shouldBeTrue("typeof event.xRotationRate == 'object'");
shouldBeTrue("typeof event.yRotationRate == 'object'");
shouldBeTrue("typeof event.zRotationRate == 'object'");
shouldBeTrue("typeof event.interval == 'object'");

window.successfullyParsed = true;
