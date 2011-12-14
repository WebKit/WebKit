description("Tests that reentrant calls to Geolocation methods from the error callback are OK.");

var mockCode = 2;
var mockMessage = 'test';

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationError(mockCode, mockMessage);
} else
    debug('This test can not be run without the LayoutTestController');

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
    mockMessage += ' repeat';

    if (window.layoutTestController)
        layoutTestController.setMockGeolocationError(mockCode, mockMessage);

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

window.jsTestIsAsync = true;
window.successfullyParsed = true;
