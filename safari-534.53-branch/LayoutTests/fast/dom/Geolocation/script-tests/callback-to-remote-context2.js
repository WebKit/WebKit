description("Tests that when a Geolocation request is made from a remote frame, and the JavaScript call chain starts from that remote frame, callbacks are made as usual.");

function onIframeReady() {
    // Make request from remote frame, with call chain starting here
    window.setTimeout(function() {
        iframe.contentWindow.navigator.geolocation.getCurrentPosition(function() {
            testPassed('Success callback invoked');
            finishJSTest();
        }, function() {
            testFailed('Error callback invoked unexpectedly');
            finishJSTest();
        });
    }, 0);
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/callback-to-remote-context-inner.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
