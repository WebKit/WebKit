description("Tests that when an exception is thrown in the success callback, the error callback is not invoked. Note that this test throws an exception which is not caught.");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100;

window.layoutTestController.setGeolocationPermission(true);
window.layoutTestController.setMockGeolocationPosition(mockLatitude,
                                                       mockLongitude,
                                                       mockAccuracy);

var position;
navigator.geolocation.getCurrentPosition(function(p) {
    position = p
    shouldBe('position.coords.latitude', 'mockLatitude');
    shouldBe('position.coords.longitude', 'mockLongitude');
    shouldBe('position.coords.accuracy', 'mockAccuracy');

    // Yield to allow for the error callback to be invoked. The timer
    // must be started before the exception is thrown.
    window.setTimeout(completeTest, 0);
    throw new Error('Exception in success callback');
}, function(e) {
    testFailed('Error callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
});

function completeTest()
{
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
}
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
