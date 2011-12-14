description("Tests that when a position is available, no callbacks are invoked until permission is denied.");

if (window.layoutTestController)
    window.layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);

function denyPermission() {
    permissionSet = true;
    if (window.layoutTestController)
        layoutTestController.setGeolocationPermission(false);
}

var error;
navigator.geolocation.getCurrentPosition(function() {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    if (permissionSet) {
        error = e;
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
        finishJSTest();
        return;
    }
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});
window.setTimeout(denyPermission, 100);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
