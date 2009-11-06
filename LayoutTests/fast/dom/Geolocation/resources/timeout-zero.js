description("Tests that when timeout is zero (and maximumAge is too), the error callback is called immediately with code TIMEOUT.");

layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100.0);

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
}, function(e) {
    error = e
    shouldBe('error.code', 'error.TIMEOUT');
    shouldBe('error.message', '"Timeout expired"');
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
}, {
    timeout: 0
});
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
