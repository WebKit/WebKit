description("Tests that reentrant calls to Geolocation methods from the success callback are OK.");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

window.layoutTestController.setGeolocationPermission(true);
window.layoutTestController.setMockGeolocationPosition(mockLatitude,
                                                       mockLongitude,
                                                       mockAccuracy);

var position;
var successCallbackInvoked = false;
navigator.geolocation.getCurrentPosition(function(p) {
    if (successCallbackInvoked) {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }
    successCallbackInvoked = true;

    position = p;
    shouldBe('position.coords.latitude', 'mockLatitude');
    shouldBe('position.coords.longitude', 'mockLongitude');
    shouldBe('position.coords.accuracy', 'mockAccuracy');
    debug('');
    continueTest();
}, function(e) {
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});

function continueTest() {
    window.layoutTestController.setMockGeolocationPosition(++mockLatitude,
                                                           ++mockLongitude,
                                                           ++mockAccuracy);

    navigator.geolocation.getCurrentPosition(function(p) {
        position = p;
        shouldBe('position.coords.latitude', 'mockLatitude');
        shouldBe('position.coords.longitude', 'mockLongitude');
        shouldBe('position.coords.accuracy', 'mockAccuracy');
        finishJSTest();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
}
window.layoutTestController.waitUntilDone();

window.jsTestIsAsync = true;
window.successfullyParsed = true;
