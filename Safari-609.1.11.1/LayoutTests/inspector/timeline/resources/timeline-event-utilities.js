function savePageData(data) {
    TestPage.dispatchEventToFrontend("SavePageData", data);
}

TestPage.registerInitializer(() => {
    InspectorTest.TimelineEvent = {};

    InspectorTest.TimelineEvent.captureTimelineWithScript = function({expression, eventType}) {
        let savePageDataPromise = InspectorTest.awaitEvent("SavePageData").then((event) => {
            return event.data;
        });

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
                .catch((error) => {
                    promise.reject(error);
                });
                return;
            }

            if (WI.timelineManager.capturingState === WI.TimelineManager.CapturingState.Inactive) {
                WI.timelineManager.removeEventListener(WI.TimelineManager.Event.CapturingStateChanged, listener);
                InspectorTest.assert(savePageDataPromise, "savePageData should have been called in the page before capturing was stopped.");
                savePageDataPromise.then((data) => {
                    promise.resolve(data);
                });
                return;
            }
        });

        InspectorTest.log("Starting Capture...");
        const newRecording = true;
        WI.timelineManager.startCapturing(newRecording);

        return promise.promise;
    }
});
