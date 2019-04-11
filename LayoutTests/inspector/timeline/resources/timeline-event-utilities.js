function savePageData(data) {
    TestPage.dispatchEventToFrontend("SavePageData", data);
}

TestPage.registerInitializer(() => {
    InspectorTest.TimelineEvent = {};

    InspectorTest.TimelineEvent.captureTimelineWithScript = function({expression, eventType}) {
        let pageRecordingData = null;

        let promise = new WI.WrappedPromise;

        let listener = WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStateChanged, (event) => {
            if (WI.timelineManager.capturingState === WI.TimelineManager.CapturingState.Active) {
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
                InspectorTest.evaluateInPage(expression)
                .catch(promise.reject);
                return;
            }

            if (WI.timelineManager.capturingState === WI.TimelineManager.CapturingState.Inactive) {
                WI.timelineManager.removeEventListener(WI.TimelineManager.Event.CapturingStateChanged, listener);
                InspectorTest.assert(pageRecordingData, "savePageData should have been called in the page before capturing was stopped.");
                promise.resolve(pageRecordingData);
                return;
            }
        });

        InspectorTest.awaitEvent("SavePageData").then((event) => {
            pageRecordingData = event.data;
        });

        InspectorTest.log("Starting Capture...");
        const newRecording = true;
        WI.timelineManager.startCapturing(newRecording);

        return promise.promise;
    }
});
