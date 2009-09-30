description("Tests Geolocation success callback using the mock service.");

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
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    window.layoutTestController.notifyDone();
}, function(e) {
    testFailed('Error callback invoked unexpectedly');
    window.layoutTestController.notifyDone();
});
window.layoutTestController.waitUntilDone();

var isAsynchronous = true;
var successfullyParsed = true;
