description("Tests that reentrant calls to Geolocation methods from the error callback are OK.");

var mockCode = 0;
var mockMessage = 'test';

window.layoutTestController.setMockGeolocationError(mockCode, mockMessage);

var error;
var errorCallbackInvoked = false;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
}, function(e) {
    if (errorCallbackInvoked) {
        testFailed('Error callback invoked unexpectedly');
        window.layoutTestController.notifyDone();
    }
    errorCallbackInvoked = true;

    error = e;
    shouldBe('error.code', 'mockCode');
    shouldBe('error.message', 'mockMessage');
    shouldBe('error.UNKNOWN_ERROR', '0');
    shouldBe('error.PERMISSION_DENIED', '1');
    shouldBe('error.POSITION_UNAVAILABLE', '2');
    shouldBe('error.TIMEOUT', '3');
    debug('');
    continueTest();
});

function continueTest() {
    mockCode += 1;
    mockMessage += ' repeat';

    window.layoutTestController.setMockGeolocationError(mockCode, mockMessage);

    navigator.geolocation.getCurrentPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        window.layoutTestController.notifyDone();
    }, function(e) {
        error = e;
        shouldBe('error.code', 'mockCode');
        shouldBe('error.message', 'mockMessage');
        shouldBe('error.UNKNOWN_ERROR', '0');
        shouldBe('error.PERMISSION_DENIED', '1');
        shouldBe('error.POSITION_UNAVAILABLE', '2');
        shouldBe('error.TIMEOUT', '3');
        debug('<br /><span class="pass">TEST COMPLETE</span>');
        window.layoutTestController.notifyDone();
    });
}
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
