description("Tests the optional properties of DeviceMotionEvent. Each property should be null if not set, or set to null or undefined.");

var event;

evalAndLog("event = document.createEvent('DeviceMotionEvent')");
shouldBeTrue("event.xAcceleration == null");
shouldBeTrue("event.yAcceleration == null");
shouldBeTrue("event.zAcceleration == null");
shouldBeTrue("event.xRotationRate == null");
shouldBeTrue("event.yRotationRate == null");
shouldBeTrue("event.zRotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, 0, 1, 2, 3, 4, 5, 6)");
shouldBeTrue("event.xAcceleration == 0");
shouldBeTrue("event.yAcceleration == 1");
shouldBeTrue("event.zAcceleration == 2");
shouldBeTrue("event.xRotationRate == 3");
shouldBeTrue("event.yRotationRate == 4");
shouldBeTrue("event.zRotationRate == 5");
shouldBeTrue("event.interval == 6");

evalAndLog("event.initDeviceMotionEvent()");
shouldBeTrue("event.xAcceleration == null");
shouldBeTrue("event.yAcceleration == null");
shouldBeTrue("event.zAcceleration == null");
shouldBeTrue("event.xRotationRate == null");
shouldBeTrue("event.yRotationRate == null");
shouldBeTrue("event.zRotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, [], [], [], [], [], [], [])");
shouldBeTrue("event.xAcceleration == 0");
shouldBeTrue("event.yAcceleration == 0");
shouldBeTrue("event.zAcceleration == 0");
shouldBeTrue("event.xRotationRate == 0");
shouldBeTrue("event.yRotationRate == 0");
shouldBeTrue("event.zRotationRate == 0");
shouldBeTrue("event.interval == 0");

evalAndLog("event.initDeviceMotionEvent('', false, false, undefined, undefined, undefined, undefined, undefined, undefined, undefined)");
shouldBeTrue("event.xAcceleration == null");
shouldBeTrue("event.yAcceleration == null");
shouldBeTrue("event.zAcceleration == null");
shouldBeTrue("event.xRotationRate == null");
shouldBeTrue("event.yRotationRate == null");
shouldBeTrue("event.zRotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, '', '', '', '', '', '', '')");
shouldBeTrue("event.xAcceleration == 0");
shouldBeTrue("event.yAcceleration == 0");
shouldBeTrue("event.zAcceleration == 0");
shouldBeTrue("event.xRotationRate == 0");
shouldBeTrue("event.yRotationRate == 0");
shouldBeTrue("event.zRotationRate == 0");
shouldBeTrue("event.interval == 0");

evalAndLog("event.initDeviceMotionEvent('', false, false, null, null, null, null, null, null, null)");
shouldBeTrue("event.xAcceleration == null");
shouldBeTrue("event.yAcceleration == null");
shouldBeTrue("event.zAcceleration == null");
shouldBeTrue("event.xRotationRate == null");
shouldBeTrue("event.yRotationRate == null");
shouldBeTrue("event.zRotationRate == null");
shouldBeTrue("event.interval == null");

window.successfullyParsed = true;
