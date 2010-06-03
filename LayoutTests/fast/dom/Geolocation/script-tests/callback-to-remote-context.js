description("Tests that when a Geolocation request is made from a remote frame, callbacks are made as usual.");

function onIframeReady() {
    // Make request from remote frame
    iframe.contentWindow.navigator.geolocation.getCurrentPosition(function() {
        testPassed('Success callback invoked');
        finishJSTest();
    }, function() {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/callback-to-remote-context-inner.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
