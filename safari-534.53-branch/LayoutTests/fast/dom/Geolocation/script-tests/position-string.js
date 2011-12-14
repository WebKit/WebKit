description("Tests formatting of position.toString().");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationPosition(mockLatitude,
                                                    mockLongitude,
                                                    mockAccuracy);
} else
    debug('This test can not be run without the LayoutTestController');

var position;
navigator.geolocation.getCurrentPosition(function(p) {
    // shouldBe can't use local variables yet.
    position = p;
    shouldBe('position.coords.latitude', 'mockLatitude');
    shouldBe('position.coords.longitude', 'mockLongitude');
    shouldBe('position.coords.accuracy', 'mockAccuracy');
    shouldBe('position.toString()', '"[object Geoposition]"');
    shouldBe('position.coords.toString()', '"[object Coordinates]"');
    finishJSTest();
}, function(e) {
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});

window.jsTestIsAsync = true;
window.successfullyParsed = true;
