description("Tests that we do not crash when a Geolocation request is made from a remote frame, which is then removed from the DOM in the error callback.");

function gc() {
    if (window.GCController) {
        GCController.collect();
        return;
    }

    for (var i = 0; i < 10000; i++)
        new String(i);
}

function onIframeReady() {
    // Make request from remote frame
    iframe.contentWindow.navigator.geolocation.getCurrentPosition(function() {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function() {
        testPassed('Error callback invoked.');
        document.body.removeChild(iframe);
        gc();
        finishJSTest();
    });
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/remove-remote-context-in-error-callback-crash-inner.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
