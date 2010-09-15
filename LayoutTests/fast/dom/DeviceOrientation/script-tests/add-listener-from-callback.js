description('Tests that adding a new event listener from a callback works as expected.');

var mockAlpha = 1.1;
var mockBeta = 2.2;
var mockGamma = 3.3;

if (window.layoutTestController)
    layoutTestController.setMockDeviceOrientation(true, mockAlpha, true, mockBeta, true, mockGamma);
else
    debug('This test can not be run without the LayoutTestController');

var deviceOrientationEvent;
function checkOrientation(event) {
    deviceOrientationEvent = event;
    shouldBe('deviceOrientationEvent.alpha', 'mockAlpha');
    shouldBe('deviceOrientationEvent.beta', 'mockBeta');
    shouldBe('deviceOrientationEvent.gamma', 'mockGamma');
}

var firstListenerEvents = 0;
function firstListener(event) {
    checkOrientation(event);
    if (++firstListenerEvents == 1)
        window.addEventListener('deviceorientation', secondListener);
    else if (firstListenerEvents > 2)
        testFailed('Too many events for first listener.');
    maybeFinishTest();
}

var secondListenerEvents = 0;
function secondListener(event) {
    checkOrientation(event);
    if (++secondListenerEvents > 1)
        testFailed('Too many events for second listener.');
    maybeFinishTest();
}

function maybeFinishTest() {
    if (firstListenerEvents == 2 && secondListenerEvents == 1)
        finishJSTest();
}

window.addEventListener('deviceorientation', firstListener);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
