description("Tests that reentrant calls to Geolocation methods from the error callback are OK.");

var mockCode = 0;
var mockMessage = 'test';

window.layoutTestController.setGeolocationPermission(true);
window.layoutTestController.setMockGeolocationError(mockCode, mockMessage);

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
    shouldBe('error.code', 'mockCode');
    shouldBe('error.message', 'mockMessage');
    debug('');
    continueTest();
});

function continueTest() {
    mockCode += 1;
    mockMessage += ' repeat';

    window.layoutTestController.setMockGeolocationError(mockCode, mockMessage);

    navigator.geolocation.getCurrentPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        error = e;
        shouldBe('error.code', 'mockCode');
        shouldBe('error.message', 'mockMessage');
        finishJSTest();
    });
}
window.layoutTestController.waitUntilDone();

window.jsTestIsAsync = true;
window.successfullyParsed = true;
