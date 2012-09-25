description("Tests that reentrant calls to Geolocation methods from the error callback are OK.");

var mockMessage = 'test';

if (window.testRunner) {
    testRunner.setGeolocationPermission(true);
    testRunner.setMockGeolocationPositionUnavailableError(mockMessage);
} else
    debug('This test can not be run without the testRunner');

var error;
var errorCallbackInvoked = false;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    if (errorCallbackInvoked) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    }
    errorCallbackInvoked = true;

    error = e;
    shouldBe('error.code', 'error.POSITION_UNAVAILABLE');
    shouldBe('error.message', 'mockMessage');
    debug('');
    continueTest();
});

function continueTest() {
    mockMessage += ' repeat';

    if (window.testRunner)
        testRunner.setMockGeolocationPositionUnavailableError(mockMessage);

    navigator.geolocation.getCurrentPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        error = e;
        shouldBe('error.code', 'error.POSITION_UNAVAILABLE');
        shouldBe('error.message', 'mockMessage');
        finishJSTest();
    });
}

window.jsTestIsAsync = true;
