description("Tests that when a request is made on a Geolocation object after its frame has been disconnected, no callbacks are made and no crash occurs.");

if (window.layoutTestController) {
    layoutTestController.setGeolocationPermission(true);
    layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100);
}

function onFirstIframeLoaded() {
    iframeGeolocation = iframe.contentWindow.navigator.geolocation;
    iframe.src = 'resources/disconnected-frame-already-inner2.html';
}

var error;
function onSecondIframeLoaded() {
    iframeGeolocation.getCurrentPosition(function () {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
    setTimeout(finishTest, 1000);
}

function finishTest() {
    debug('Method called on Geolocation object with disconnected Frame.');
    finishJSTest();
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/disconnected-frame-already-inner1.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
