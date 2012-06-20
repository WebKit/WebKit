description("Tests that when timeout is zero (and maximumAge is too), the error callback is called immediately with code TIMEOUT.");

if (window.testRunner)
    testRunner.setMockGeolocationPosition(51.478, -0.166, 100.0);

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    error = e;
    shouldBe('error.code', 'error.TIMEOUT');
    shouldBe('error.message', '"Timeout expired"');
    finishJSTest();
}, {
    timeout: 0
});

window.jsTestIsAsync = true;
