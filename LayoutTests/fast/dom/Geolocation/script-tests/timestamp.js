description("Tests that Geoposition timestamps are well-formed (non-zero and in the same units as Date.getTime).");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationPosition(mockLatitude, mockLongitude, mockAccuracy);
}

var now = new Date().getTime();
shouldBeTrue('now != 0');
var t = null;
var then = null;

function checkPosition(p) {
    t = p.timestamp;
    var d = new Date();
    then = d.getTime();
    shouldBeTrue('t != 0');
    shouldBeTrue('then != 0');
    shouldBeTrue('now - 1 <= t'); // Avoid rounding errors
    if (now - 1 > t) {
        debug("  now - 1 = " + (now-1));
        debug("  t = " + t);
    }
    shouldBeTrue('t <= then + 1'); // Avoid rounding errors
    finishJSTest();
}

navigator.geolocation.getCurrentPosition(checkPosition);
window.jsTestIsAsync = true;
window.successfullyParsed = true;
