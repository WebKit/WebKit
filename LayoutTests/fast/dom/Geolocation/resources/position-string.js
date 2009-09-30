description("Tests formatting of position.toString().");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

window.layoutTestController.setGeolocationPermission(true);
window.layoutTestController.setMockGeolocationPosition(mockLatitude,
                                                       mockLongitude,
                                                       mockAccuracy);

var position;
navigator.geolocation.getCurrentPosition(function(p) {
    // shouldBe can't use local variables yet.
    position = p
    shouldBe('position.coords.latitude', 'mockLatitude');
    shouldBe('position.coords.longitude', 'mockLongitude');
    shouldBe('position.coords.accuracy', 'mockAccuracy');
    shouldBe('position.toString()', '"[object Geoposition]"');
    shouldBe('position.coords.toString()', '"[object Coordinates]"');
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
}, function(e) {
    testFailed('Error callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
});
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
