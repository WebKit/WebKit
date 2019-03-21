function finishRecording(data) {
    TestPage.addResult("Finish recording...");
    TestPage.dispatchEventToFrontend("FinishRecording", data);
}

TestPage.registerInitializer(() => {
    InspectorTest.TimelineEvent = {};

    InspectorTest.TimelineEvent.captureTimelineWithScript = function(expression) {
        let pageRecordingData = null;

        InspectorTest.log("Starting Capture...");
        const newRecording = true;
        WI.timelineManager.startCapturing(newRecording);

        let promises = [];

        promises.push(WI.timelineManager.awaitEvent(WI.TimelineManager.Event.CapturingStopped));

        promises.push(InspectorTest.awaitEvent("FinishRecording").then((event) => {
            InspectorTest.log("Stopping Capture...");
            pageRecordingData = event.data;
            WI.timelineManager.stopCapturing();
        }));

        InspectorTest.log("Evaluating...");
        promises.push(InspectorTest.evaluateInPage(expression));

        return Promise.all(promises).then(() => pageRecordingData);
    }
});
