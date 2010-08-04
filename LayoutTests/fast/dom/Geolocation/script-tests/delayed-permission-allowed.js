description("Tests that when a position is available, no callbacks are invoked until permission is allowed.");

if (window.layoutTestController)
    window.layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);

function allowPermission() {
    permissionSet = true;
    if (window.layoutTestController)
        layoutTestController.setGeolocationPermission(true);
}

navigator.geolocation.getCurrentPosition(function() {
    if (permissionSet) {
        testPassed('Success callback invoked');
        finishJSTest();
        return;
    }
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function() {
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});
window.setTimeout(allowPermission, 100);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
