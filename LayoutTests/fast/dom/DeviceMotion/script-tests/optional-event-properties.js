description("Tests the optional properties of DeviceMotionEvent. Each property should be null if not set, or set to null or undefined.");

function ObjectThrowingException() {};
ObjectThrowingException.prototype.valueOf = function() { throw new Error('valueOf threw exception'); }
ObjectThrowingException.prototype.__defineGetter__("x", function() { throw new Error('x getter exception'); });
ObjectThrowingException.prototype.__defineGetter__("alpha", function() { throw new Error('alpha getter exception'); });
var objectThrowingException = new ObjectThrowingException();

function testException(expression, expectedException)
{
    shouldThrow(expression, '(function() { return "' + expectedException + '"; })();');
}

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

testException("event.initDeviceMotionEvent('', false, false, objectThrowingException, {x: 3, z: 5}, {gamma: 8, beta: 7}, 9)", "Error: x getter exception");
testException("event.initDeviceMotionEvent('', false, false, {x: 0, y: 1, z: 2}, objectThrowingException, {gamma: 8, beta: 7}, 9)", "Error: x getter exception");
testException("event.initDeviceMotionEvent('', false, false, {x: 0, y: 1, z: 2}, {x: 3, z: 5}, objectThrowingException, 9)", "Error: alpha getter exception");

testException("event.initDeviceMotionEvent('', false, false, {x: objectThrowingException, y: 1, z: 2}, {x: 3, y: 4, z: 5}, {alpha: 6, beta: 7, gamma: 8}, 9)", "Error: valueOf threw exception");
testException("event.initDeviceMotionEvent('', false, false, {x: 0, y: 1, z: 2}, {x: 3, y: objectThrowingException, z: 5}, {alpha: 6, beta: 7, gamma: 8}, 9)", "Error: valueOf threw exception");
testException("event.initDeviceMotionEvent('', false, false, {x: 0, y: 1, z: 2}, {x: 3, y: 4, z: 5}, {alpha: 6, beta: 7, gamma: objectThrowingException}, 9)", "Error: valueOf threw exception");

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

evalAndLog("event.initDeviceMotionEvent('', false, false, [], [], [], [])");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == 0");

evalAndLog("event.initDeviceMotionEvent('', false, false, undefined, undefined, undefined, undefined)");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, '', '', '', '')");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == 0");

evalAndLog("event.initDeviceMotionEvent('', false, false, null, null, null, null)");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, {x: null, y: null, z: null}, {x: null, y: null, z: null}, {alpha: null, beta: null, gamma: null}, null)");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, {x: null, y: null, z: 1}, {x: null, y: null, z: 2}, {alpha: null, beta: null, gamma: 3}, null)");
shouldBeTrue("event.acceleration.x == null");
shouldBeTrue("event.acceleration.y == null");
shouldBeTrue("event.acceleration.z == 1");
shouldBeTrue("event.accelerationIncludingGravity.x == null");
shouldBeTrue("event.accelerationIncludingGravity.y == null");
shouldBeTrue("event.accelerationIncludingGravity.z == 2");
shouldBeTrue("event.rotationRate.alpha == null");
shouldBeTrue("event.rotationRate.beta == null");
shouldBeTrue("event.rotationRate.gamma == 3");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, {x: undefined, y: undefined, z: undefined}, {x: undefined, y: undefined, z: undefined}, {alpha: undefined, beta: undefined, gamma: undefined}, undefined)");
shouldBeTrue("event.acceleration == null");
shouldBeTrue("event.accelerationIncludingGravity == null");
shouldBeTrue("event.rotationRate == null");
shouldBeTrue("event.interval == null");

evalAndLog("event.initDeviceMotionEvent('', false, false, {x: undefined, y: undefined, z: 1}, {x: undefined, y: undefined, z: 2}, {alpha: undefined, beta: undefined, gamma: 3}, undefined)");
shouldBeTrue("event.acceleration.x == null");
shouldBeTrue("event.acceleration.y == null");
shouldBeTrue("event.acceleration.z == 1");
shouldBeTrue("event.accelerationIncludingGravity.x == null");
shouldBeTrue("event.accelerationIncludingGravity.y == null");
shouldBeTrue("event.accelerationIncludingGravity.z == 2");
shouldBeTrue("event.rotationRate.alpha == null");
shouldBeTrue("event.rotationRate.beta == null");
shouldBeTrue("event.rotationRate.gamma == 3");
shouldBeTrue("event.interval == null");

window.successfullyParsed = true;
