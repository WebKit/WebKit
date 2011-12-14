description("Tests that when a request is made on a Geolocation object and its Frame is disconnected before a callback is made, the error callback is invoked with the correct error message.");

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);
}

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
        shouldBe('error.code', '2');
        shouldBe('error.message', '"Geolocation cannot be used in frameless documents"');
        finishJSTest();
    });
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/disconnected-frame-inner.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
