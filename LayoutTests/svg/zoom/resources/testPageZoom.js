if (window.layoutTestController && window.eventSender) {
    layoutTestController.waitUntilDone();

    // Give DRT a chance to layout the document with its initial size,
    // before zooming the page. This also tests repainting as side-effect.
    setTimeout(function() {
        for (i = 0; i < zoomCount; ++i) {
            if (window.shouldZoomOut)
                eventSender.zoomPageOut();
            else
                eventSender.zoomPageIn();
        }

        if (window.postZoomCallback) {
            window.setTimeout(function() {
                window.postZoomCallback();
                completeDynamicTest();
            }, 0);
        } else {
            setTimeout(function() { layoutTestController.notifyDone(); }, 0);
        }
    }, 0);
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
