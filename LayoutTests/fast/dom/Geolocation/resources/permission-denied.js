description("Tests Geolocation when permission is denied, using the mock service.");

window.layoutTestController.setGeolocationPermission(false);
window.layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
}, function(e) {
    error = e
    shouldBe('error.code', 'error.PERMISSION_DENIED');
    shouldBe('error.message', '"User denied Geolocation"');
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
});
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
