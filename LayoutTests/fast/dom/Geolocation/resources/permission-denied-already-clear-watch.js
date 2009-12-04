description("Tests that when Geolocation permission has been denied prior to a call to watchPosition, and the watch is cleared in the error callback, there is no crash. This a regression test for https://bugs.webkit.org/show_bug.cgi?id=32111.");

// Prime the Geolocation instance by denying permission.
window.layoutTestController.setGeolocationPermission(false);
window.layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
}, function(e) {
    error = e
    shouldBe('error.code', 'error.PERMISSION_DENIED');
    shouldBe('error.message', '"User denied Geolocation"');
    debug('');
    continueTest();
});

function continueTest()
{
    // Make another request, with permission already denied.
    var watchId = navigator.geolocation.watchPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        window.layoutTestController.notifyDone();
    }, function(e) {
        error = e
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
        navigator.geolocation.clearWatch(watchId);
        window.setTimeout(completeTest, 0);
    });
}

function completeTest()
{
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
}
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
