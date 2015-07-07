description("Tests that when a request is made on a Geolocation object and its Frame is disconnected before a callback is made, the error callback is invoked with the correct error message.");

if (window.testRunner) {
    testRunner.setGeolocationPermission(true);
    testRunner.setMockGeolocationPosition(51.478, -0.166, 100);
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
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
    setTimeout(function() {
        testPassed('No callbacks invoked');
        finishJSTest();
    }, 100);
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/disconnected-frame-inner.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
