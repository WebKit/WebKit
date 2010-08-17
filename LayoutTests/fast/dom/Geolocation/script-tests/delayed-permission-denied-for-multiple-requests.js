description("Tests that when multiple requests are waiting for permission, no callbacks are invoked until permission is denied.");

if (window.layoutTestController)
    window.layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);

function denyPermission() {
    permissionSet = true;
    if (window.layoutTestController)
        layoutTestController.setGeolocationPermission(false);
}

var watchCallbackInvoked = false;
var oneShotCallbackInvoked = false;
var error;

navigator.geolocation.watchPosition(function() {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    if (permissionSet) {
        error = e;
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
        watchCallbackInvoked = true;
        maybeFinishTest();
        return;
    }
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});

navigator.geolocation.getCurrentPosition(function() {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    if (permissionSet) {
        error = e;
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
        oneShotCallbackInvoked = true;
        maybeFinishTest();        
        return;
    }
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});
window.setTimeout(denyPermission, 100);

function maybeFinishTest() {
    if (watchCallbackInvoked && oneShotCallbackInvoked)
        finishJSTest();
}

window.jsTestIsAsync = true;
window.successfullyParsed = true;
