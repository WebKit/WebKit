TestPage.registerInitializer(() => {
    window.pageRecordingData = null;

    window.captureTimelineWithScript = function(expression) {
        pageRecordingData = null;

        InspectorTest.log("Starting Capture...");
        const newRecording = true;
        WI.timelineManager.startCapturing(newRecording);
        InspectorTest.evaluateInPage(expression);
        InspectorTest.awaitEvent("FinishRecording").then((event) => {
            InspectorTest.log("Stopping Capture...");
            pageRecordingData = event.data;
            WI.timelineManager.stopCapturing();
        });

        return WI.timelineManager.awaitEvent(WI.TimelineManager.Event.CapturingStopped);
    }
});

function finishRecording(data) {
    TestPage.dispatchEventToFrontend("FinishRecording", data);
}
