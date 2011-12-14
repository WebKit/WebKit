description("Tests that the PositionOptions.maximumAge parameter is correctly applied.");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

var mockCode = 2;
var mockMessage = 'test';

var position;
var error;

function checkPosition(p) {
    debug('');
    position = p;
    shouldBe('position.coords.latitude', 'mockLatitude');
    shouldBe('position.coords.longitude', 'mockLongitude');
    shouldBe('position.coords.accuracy', 'mockAccuracy');
}

function checkError(e) {
    debug('');
    error = e;
    shouldBe('error.code', 'mockCode');
    shouldBe('error.message', 'mockMessage');
}

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationPosition(mockLatitude, mockLongitude, mockAccuracy);
} else
    debug('This test can not be run without the LayoutTestController');

// Initialize the cached Position
navigator.geolocation.getCurrentPosition(function(p) {
    checkPosition(p);
    testZeroMaximumAge();
}, function(e) {
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});

function testZeroMaximumAge() {
    // Update the position provided by the mock service.
    if (window.layoutTestController)
        layoutTestController.setMockGeolocationPosition(++mockLatitude, ++mockLongitude, ++mockAccuracy);
    // The default maximumAge is zero, so we expect the updated position from the service.
    navigator.geolocation.getCurrentPosition(function(p) {
        checkPosition(p);
        testNonZeroMaximumAge();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
}

function testNonZeroMaximumAge() {
    // Update the mock service to report an error.
    if (window.layoutTestController)
        layoutTestController.setMockGeolocationError(mockCode, mockMessage);
    // The maximumAge is non-zero, so we expect the cached position, not the error from the service.
    navigator.geolocation.getCurrentPosition(function(p) {
        checkPosition(p);
        testZeroMaximumAgeError();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    }, {maximumAge: 1000});
}

function testZeroMaximumAgeError() {
    // The default maximumAge is zero, so we expect the error from the service.
    navigator.geolocation.getCurrentPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        checkError(e);
        finishJSTest();
    });
}

window.jsTestIsAsync = true;
window.successfullyParsed = true;
