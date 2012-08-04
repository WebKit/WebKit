description("Tests that watchPosition correctly reports position updates and errors from the Geolocation service.");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

var mockCode = 2;
var mockMessage = 'test';

var position;
var error;

function checkPosition(p) {
    position = p;
    shouldBe('position.coords.latitude', 'mockLatitude');
    shouldBe('position.coords.longitude', 'mockLongitude');
    shouldBe('position.coords.accuracy', 'mockAccuracy');
    debug('');
}

function checkError(e) {
    error = e;
    shouldBe('error.code', 'mockCode');
    shouldBe('error.message', 'mockMessage');
    debug('');
}

if (window.testRunner) {
    testRunner.setGeolocationPermission(true);
    testRunner.setMockGeolocationPosition(mockLatitude, mockLongitude, mockAccuracy);
} else
    debug('This test can not be run without the testRunner');

var state = 0;
navigator.geolocation.watchPosition(function(p) {
    switch (state++) {
        case 0:
            checkPosition(p);
            if (window.testRunner)
                testRunner.setMockGeolocationPosition(++mockLatitude, ++mockLongitude, ++mockAccuracy);
            break;
        case 1:
            checkPosition(p);
            if (window.testRunner)
                testRunner.setMockGeolocationError(mockCode, mockMessage);
            break;
        case 3:
            checkPosition(p);
            finishJSTest();
            break;
        default:
            testFailed('Success callback invoked unexpectedly');
            finishJSTest();
    }
}, function(e) {
    switch (state++) {
        case 2:
            checkError(e);
            if (window.testRunner)
                testRunner.setMockGeolocationPosition(++mockLatitude, ++mockLongitude, ++mockAccuracy);
            break;
        default:
            testFailed('Error callback invoked unexpectedly');
            finishJSTest();
    }
});

window.jsTestIsAsync = true;
