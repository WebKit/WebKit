description("Tests for a crash when clearWatch() is called with a zero ID.<br><br>We call clearWatch() with a request in progress then navigate the page. This accesses the watchers map during cleanup and triggers the crash. This page should not be visible when the test completes.");

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);
} else
    debug('This test can not be run without the LayoutTestController');

document.body.onload = function() {
    navigator.geolocation.watchPosition(function() {});
    navigator.geolocation.clearWatch(0);
    location = "data:text/html,TEST COMPLETE<script>if(window.layoutTestController) layoutTestController.notifyDone();</script>";
}

window.jsTestIsAsync = true;
