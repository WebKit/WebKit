function savePageData(data) {
    TestPage.dispatchEventToFrontend("SavePageData", data);
}

TestPage.registerInitializer(() => {
    InspectorTest.TimelineEvent = {};

    InspectorTest.TimelineEvent.captureTimelineWithScript = function({expression, eventType}) {
        let pageRecordingData = null;

        let promise = new WI.WrappedPromise;

        WI.timelineManager.awaitEvent(WI.TimelineManager.Event.CapturingStopped).then((capturingStoppedEvent) => {
            InspectorTest.assert(pageRecordingData, "savePageData should have been called in the page before capturing was stopped.");
            promise.resolve(pageRecordingData);
        });

        InspectorTest.awaitEvent("SavePageData").then((event) => {
            pageRecordingData = event.data;
        });

        WI.timelineManager.awaitEvent(WI.TimelineManager.Event.CapturingStarted).then((capturingStartedEvent) => {
            let recording = WI.timelineManager.activeRecording;
            let scriptTimeline = recording.timelines.get(WI.TimelineRecord.Type.Script);

            let recordAddedListener = scriptTimeline.addEventListener(WI.Timeline.Event.RecordAdded, (recordAddedEvent) => {
                let {record} = recordAddedEvent.data;
                if (record.eventType !== eventType)
                    return;

                scriptTimeline.removeEventListener(WI.Timeline.Event.RecordAdded, recordAddedListener);

                InspectorTest.log("Stopping Capture...");
                WI.timelineManager.stopCapturing();
            });

            InspectorTest.log("Evaluating...");
            return InspectorTest.evaluateInPage(expression);
        });

        InspectorTest.log("Starting Capture...");
        const newRecording = true;
        WI.timelineManager.startCapturing(newRecording);

        return promise.promise;
    }
});
