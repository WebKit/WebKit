function repaintTest() {
    if (!window.eventSender)
        return;

    for (i = 0; i < zoomCount; ++i) {
        if (window.shouldZoomOut)
            eventSender.textZoomOut();
        else
            eventSender.textZoomIn();
    }

    if (window.testRunner)
        testRunner.notifyDone();
}
