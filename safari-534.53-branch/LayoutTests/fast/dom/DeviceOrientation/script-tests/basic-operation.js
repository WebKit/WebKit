description('Tests the basic operation of DeviceOrientation using the mock.');

var mockAlpha = 1.1;
var mockBeta = 2.2;
var mockGamma = 3.3;

if (window.layoutTestController)
    layoutTestController.setMockDeviceOrientation(true, mockAlpha, true, mockBeta, true, mockGamma);
else
    debug('This test can not be run without the LayoutTestController');

var deviceOrientationEvent;
window.addEventListener('deviceorientation', function(e) {
    deviceOrientationEvent = e;
    shouldBe('deviceOrientationEvent.alpha', 'mockAlpha');
    shouldBe('deviceOrientationEvent.beta', 'mockBeta');
    shouldBe('deviceOrientationEvent.gamma', 'mockGamma');
    finishJSTest();
});

window.jsTestIsAsync = true;
window.successfullyParsed = true;
