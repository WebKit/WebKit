description('Tests using null values for some of the event properties.');

var mockEvent;
function setMockOrientation(alpha, beta, gamma) {
    mockEvent = {alpha: alpha, beta: beta, gamma: gamma};
    if (window.layoutTestController)
        layoutTestController.setMockDeviceOrientation(
            null != mockEvent.alpha, null == mockEvent.alpha ? 0 : mockEvent.alpha,
            null != mockEvent.beta, null == mockEvent.beta ? 0 : mockEvent.beta,
            null != mockEvent.gamma, null == mockEvent.gamma ? 0 : mockEvent.gamma);
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

    setMockOrientation(1.1, null, null);
    window.addEventListener('deviceorientation', secondListener);
}

function secondListener(event) {
    checkOrientation(event);
    window.removeEventListener('deviceorientation', secondListener);

    setMockOrientation(null, 2.2, null);
    window.addEventListener('deviceorientation', thirdListener);
}

function thirdListener(event) {
    checkOrientation(event);
    window.removeEventListener('deviceorientation', thirdListener);

    setMockOrientation(null, null, 3.3);
    window.addEventListener('deviceorientation', fourthListener);
}

function fourthListener(event) {
    checkOrientation(event);
    finishJSTest();
}

setMockOrientation(null, null, null);
window.addEventListener('deviceorientation', firstListener);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
