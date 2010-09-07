description('Tests that updates to the orientation causes new events to fire.');

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

function firstListener(event) {
    checkOrientation(event);
    window.removeEventListener('deviceorientation', firstListener);

    setMockOrientation(11.1, 22.2, 33.3);
    window.addEventListener('deviceorientation', updateListener);
}

function updateListener(event) {
    checkOrientation(event);
    finishJSTest();
}

setMockOrientation(1.1, 2.2, 3.3);
window.addEventListener('deviceorientation', firstListener);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
