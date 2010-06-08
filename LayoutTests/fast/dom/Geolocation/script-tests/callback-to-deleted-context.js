description("Tests that when a Geolocation request is made from a remote frame, and that frame's script context goes away before the Geolocation callback is made, the callback is not made. If the callback is attempted, a crash will occur.");

function onFirstIframeLoaded() {
    iframe.src = 'resources/callback-to-deleted-context-inner2.html';
}

function onSecondIframeLoaded() {
    // Wait for the callbacks to be invoked
    window.setTimeout(function() {
        testPassed('No callbacks invoked');
        finishJSTest();
    }, 100);
}

var iframe = document.createElement('iframe');
iframe.src = 'resources/callback-to-deleted-context-inner1.html';
document.body.appendChild(iframe);

window.jsTestIsAsync = true;
window.successfullyParsed = true;
