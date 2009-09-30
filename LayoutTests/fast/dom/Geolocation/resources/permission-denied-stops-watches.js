description("Tests that when Geolocation permission is denied, watches are stopped, as well as one-shots.");

// Configure the mock Geolocation service to report a position to cause permission
// to be requested, then deny it.
window.layoutTestController.setGeolocationPermission(false);
window.layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100.0);

var error;
var errorCallbackInvoked = false;
navigator.geolocation.watchPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
}, function(e) {
    if (errorCallbackInvoked) {
        testFailed('Error callback invoked unexpectedly : ' + error.message);
        window.layoutTestController.notifyDone();
    }
    errorCallbackInvoked = true;

    error = e
    shouldBe('error.code', 'error.PERMISSION_DENIED');
    shouldBe('error.message', '"User disallowed Geolocation"');
    shouldBe('error.UNKNOWN_ERROR', '0');
    shouldBe('error.PERMISSION_DENIED', '1');
    shouldBe('error.POSITION_UNAVAILABLE', '2');
    shouldBe('error.TIMEOUT', '3');

    // Update the mock Geolocation service to report a new position, then
    // yield to allow a chance for the success callback to be invoked.
    window.layoutTestController.setMockGeolocationPosition(55.478, -0.166, 100);
    window.setTimeout(completeTest, 0);
});

function completeTest()
{
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
}
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
