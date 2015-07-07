description("Tests Geolocation when permission is denied, using the mock service.");

if (window.testRunner) {
    testRunner.setGeolocationPermission(false);
    testRunner.setMockGeolocationPosition(51.478, -0.166, 100.0);
} else
    debug('This test can not be run without the testRunner');

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    error = e;
    shouldBe('error.code', 'error.PERMISSION_DENIED');
    shouldBe('error.message', '"User denied Geolocation"');
    finishJSTest();
});

window.jsTestIsAsync = true;
