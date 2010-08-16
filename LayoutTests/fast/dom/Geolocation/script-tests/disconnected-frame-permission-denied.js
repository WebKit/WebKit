description("Tests that when a request is made on a Geolocation object, permission is denied and its Frame is disconnected before a callback is made, the error callback is invoked with PERMISSION_DENIED.");

if (window.layoutTestController)
    layoutTestController.setGeolocationPermission(false);

function onIframeLoaded() {
    iframeGeolocation = iframe.contentWindow.navigator.geolocation;
    iframe.src = 'data:text/html,This frame should be visible when the test completes';
}

var error;
function onIframeUnloaded() {
    iframeGeolocation.getCurrentPosition(function () {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        error = e;
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
        finishJSTest();
    });
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/disconnected-frame-inner.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
