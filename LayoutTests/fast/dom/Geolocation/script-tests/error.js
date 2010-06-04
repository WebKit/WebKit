description("Tests Geolocation error callback using the mock service.");

var mockCode = 2;
var mockMessage = "debug";

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationError(mockCode, mockMessage);
} else
    debug('This test can not be run without the LayoutTestController');

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    error = e;
    shouldBe('error.code', 'mockCode');
    shouldBe('error.message', 'mockMessage');
    shouldBe('error.UNKNOWN_ERROR', 'undefined');
    shouldBe('error.PERMISSION_DENIED', '1');
    shouldBe('error.POSITION_UNAVAILABLE', '2');
    shouldBe('error.TIMEOUT', '3');
    finishJSTest();
});

window.jsTestIsAsync = true;
window.successfullyParsed = true;
