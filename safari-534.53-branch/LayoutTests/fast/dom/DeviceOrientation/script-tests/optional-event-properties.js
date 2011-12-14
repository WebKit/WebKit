description('Tests the optional properties of DeviceOrientationEvent. Each property should be null if not set, or set to null or undefined.');

var event;

evalAndLog("event = document.createEvent('DeviceOrientationEvent')");
shouldBeTrue("event.alpha == null");
shouldBeTrue("event.beta == null");
shouldBeTrue("event.gamma == null");

evalAndLog("event.initDeviceOrientationEvent('', false, false, 0, 1, 2)");
shouldBeTrue("event.alpha == 0");
shouldBeTrue("event.beta == 1");
shouldBeTrue("event.gamma == 2");

evalAndLog("event.initDeviceOrientationEvent()");
shouldBeTrue("event.alpha == null");
shouldBeTrue("event.beta == null");
shouldBeTrue("event.gamma == null");

evalAndLog("event.initDeviceOrientationEvent('', false, false, [], [], [])");
shouldBeTrue("event.alpha == 0");
shouldBeTrue("event.beta == 0");
shouldBeTrue("event.gamma == 0");

evalAndLog("event.initDeviceOrientationEvent('', false, false, undefined, undefined, undefined)");
shouldBeTrue("event.alpha == null");
shouldBeTrue("event.beta == null");
shouldBeTrue("event.gamma == null");

evalAndLog("event.initDeviceOrientationEvent('', false, false, '', '', '')");
shouldBeTrue("event.alpha == 0");
shouldBeTrue("event.beta == 0");
shouldBeTrue("event.gamma == 0");

evalAndLog("event.initDeviceOrientationEvent('', false, false, null, null, null)");
shouldBeTrue("event.alpha == null");
shouldBeTrue("event.beta == null");
shouldBeTrue("event.gamma == null");

window.successfullyParsed = true;
