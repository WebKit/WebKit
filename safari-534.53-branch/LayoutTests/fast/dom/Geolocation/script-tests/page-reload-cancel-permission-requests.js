description("Tests that when a page is reloaded, the frame is properly detached from the Geolocation object " +
            "to ensure that no permission requests are in progress.");

window.jsTestIsAsync = true;

var numPendingRequests;
var isReload = false;

if ("#reload" == location.hash)
    isReload = true;

if (window.layoutTestController) {
    numPendingRequests = layoutTestController.numberOfPendingGeolocationPermissionRequests();
    shouldBe('numPendingRequests', '0');

    if (isReload)
        finishJSTest();
}

if (!isReload) {
    // Kick off a position request and then reload the page, this should set up
    // a permission request. Permission should be undecided at this point, so the
    // permission request should still be outstanding by page reload.

    function onIframeReady()
    {
        // Make request on remote frame's Geolocation object.
        iframe.contentWindow.navigator.geolocation.getCurrentPosition(
            function(p) {
                testFailed('Permission should not be determined for this page: ' + p);
                finishJSTest();
            });

        location.hash = '#reload';
        location.reload();
    }

    debug("Create IFrame");
    var iframe = document.createElement('iframe');
    iframe.src = 'resources/page-reload-cancel-permission-requests-inner.html';
    document.body.appendChild(iframe);
}

window.successfullyParsed = true;
