description('Tests that adding a new event listener from a callback works as expected.');

var mockEvent;
function setMockOrientation(alpha, beta, gamma) {
    mockEvent = {alpha: alpha, beta: beta, gamma: gamma};
    if (window.layoutTestController)
        layoutTestController.setMockDeviceOrientation(true, mockEvent.alpha, true, mockEvent.beta, true, mockEvent.gamma);
    else
        debug('This test can not be run without the LayoutTestController');
}

var deviceOrientationEvent;
function checkOrientation(event) {
    deviceOrientationEvent = event;
    shouldBe('deviceOrientationEvent.alpha', 'mockEvent.alpha');
    shouldBe('deviceOrientationEvent.beta', 'mockEvent.beta');
    shouldBe('deviceOrientationEvent.gamma', 'mockEvent.gamma');
}

var firstListenerEvents = 0;
function firstListener(event) {
    checkOrientation(event);
    if (++firstListenerEvents == 1)
        setMockOrientation(11.1, 22.2, 33.3);
    else if (firstListenerEvents > 2)
        testFailed('Too many events for first listener.');
    window.addEventListener('deviceorientation', secondListener);
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

setMockOrientation(1.1, 2.2, 3.3);
window.addEventListener('deviceorientation', firstListener);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
