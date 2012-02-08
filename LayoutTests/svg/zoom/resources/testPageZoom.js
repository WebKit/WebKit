function repaintTest() {
    if (!window.eventSender)
        return;

    for (i = 0; i < zoomCount; ++i) {
        if (window.shouldZoomOut)
            eventSender.zoomPageOut();
        else
            eventSender.zoomPageIn();
    }

    if (!window.postZoomCallback)
        return;

    if (window.layoutTestController)
        layoutTestController.waitUntilDone();

    window.postZoomCallback();
    completeDynamicTest();
}

function completeDynamicTest() {
    var script = document.createElement("script");

    script.onload = function() {
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    };

    script.src = "../../../fast/js/resources/js-test-post.js";
    successfullyParsed = true;
    document.body.appendChild(script);
}
