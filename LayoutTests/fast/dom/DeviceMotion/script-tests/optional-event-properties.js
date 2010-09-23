description("Tests the optional properties of DeviceMotionEvent. Each property should be null if not set, or set to null or undefined.");

var event;

evalAndLog("event = document.createEvent('DeviceMotionEvent')");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, {x: 0, y: 1, z: 2}, {x: 3, y: 4, z: 5}, {alpha: 6, beta: 7, gamma: 8}, 9)");
shouldBeTrue("event.acceleration.x == 0");
shouldBeTrue("event.acceleration.y == 1");
shouldBeTrue("event.acceleration.z == 2");
shouldBeTrue("event.accelerationIncludingGravity.x == 3");
shouldBeTrue("event.accelerationIncludingGravity.y == 4");
shouldBeTrue("event.accelerationIncludingGravity.z == 5");
shouldBeTrue("event.rotationRate.alpha == 6");
shouldBeTrue("event.rotationRate.beta == 7");
shouldBeTrue("event.rotationRate.gamma == 8");
shouldBeTrue("event.interval == 9");

evalAndLog("event.initDeviceMotionEvent('', false, false, {y: 1, x: 0}, {x: 3, z: 5}, {gamma: 8, beta: 7}, 9)");
shouldBeTrue("event.acceleration.x == 0");
shouldBeTrue("event.acceleration.y == 1");
shouldBeTrue("event.acceleration.z == null");
shouldBeTrue("event.accelerationIncludingGravity.x == 3");
shouldBeTrue("event.accelerationIncludingGravity.y == null");
shouldBeTrue("event.accelerationIncludingGravity.z == 5");
shouldBeTrue("event.rotationRate.alpha == null");
shouldBeTrue("event.rotationRate.beta == 7");
shouldBeTrue("event.rotationRate.gamma == 8");
shouldBeTrue("event.interval == 9");

evalAndLog("event.initDeviceMotionEvent()");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, [], [], [], [], [], [], [])");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == 0");

evalAndLog("event.initDeviceMotionEvent('', false, false, undefined, undefined, undefined, undefined, undefined, undefined, undefined)");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, '', '', '', '', '', '', '')");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == 0");

evalAndLog("event.initDeviceMotionEvent('', false, false, null, null, null, null, null, null, null)");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

window.successfullyParsed = true;
