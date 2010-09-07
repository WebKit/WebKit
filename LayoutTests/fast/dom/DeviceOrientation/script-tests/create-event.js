description('Tests that document.createEvent() works with DeviceOrientationEvent.');

var event = document.createEvent('DeviceOrientationEvent');

shouldBeTrue("typeof event == 'object'");

shouldBeTrue("'type' in event");
shouldBeTrue("'bubbles' in event");
shouldBeTrue("'cancelable' in event");
shouldBeTrue("'alpha' in event");
shouldBeTrue("'beta' in event");
shouldBeTrue("'gamma' in event");

shouldBeTrue("typeof event.type == 'string'");
shouldBeTrue("typeof event.bubbles == 'boolean'");
shouldBeTrue("typeof event.cancelable == 'boolean'");
shouldBeTrue("typeof event.alpha == 'object'");
shouldBeTrue("typeof event.beta == 'object'");
shouldBeTrue("typeof event.gamma == 'object'");

window.successfullyParsed = true;
