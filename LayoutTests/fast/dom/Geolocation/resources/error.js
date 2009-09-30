description("Tests Geolocation error callback using the mock service.");

var mockCode = 0;
var mockMessage = "debug";

window.layoutTestController.setMockGeolocationError(mockCode, mockMessage);

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
}, function(e) {
    error = e
    shouldBe('error.code', 'mockCode');
    shouldBe('error.message', 'mockMessage');
    shouldBe('error.UNKNOWN_ERROR', '0');
    shouldBe('error.PERMISSION_DENIED', '1');
    shouldBe('error.POSITION_UNAVAILABLE', '2');
    shouldBe('error.TIMEOUT', '3');
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
});
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
