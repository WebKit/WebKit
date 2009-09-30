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
        window.layoutTestController.notifyDone();
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
    window.layoutTestController.notifyDone();
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
        debug('<br /><span class="pass">TEST COMPLETE</span>');
        window.layoutTestController.notifyDone();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        window.layoutTestController.notifyDone();
    });
}
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
