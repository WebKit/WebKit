function savePageData(data) {
    TestPage.dispatchEventToFrontend("SavePageData", data);
}

TestPage.registerInitializer(() => {
    InspectorTest.TimelineEvent = {};

    function captureTimeline(expression, silent, callback) {
        let savePageDataPromise = InspectorTest.awaitEvent("SavePageData").then((event) => event.data);

        let promise = new Promise((resolve, reject) => {
            let listener = WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStateChanged, (event) => {
                if (event.data.capturingState === WI.TimelineManager.CapturingState.Active) {
                    callback(function() {
                        if (!silent)
                            InspectorTest.log("Stopping Capture...");
                        WI.timelineManager.stopCapturing();
                    });

                    if (!silent)
                        InspectorTest.log("Evaluating...");
                    InspectorTest.evaluateInPage(expression).catch(reject);
                    return;
                }

                if (event.data.capturingState === WI.TimelineManager.CapturingState.Inactive) {
                    WI.timelineManager.removeEventListener(WI.TimelineManager.Event.CapturingStateChanged, listener);
                    InspectorTest.assert(savePageDataPromise, "savePageData should have been called in the page before capturing was stopped.");
                    savePageDataPromise.then(resolve);
                    return;
                }
            });
        });

        if (!silent)
            InspectorTest.log("Starting Capture...");
        const newRecording = true;
        WI.timelineManager.startCapturing(newRecording);

        return promise;
    }

    InspectorTest.TimelineEvent.captureTimelineWithScript = function({expression, eventType, timelineType, silent = false}) {
        return captureTimeline(expression, silent, function(callback) {
            let recording = WI.timelineManager.activeRecording;
            let timeline = recording.timelines.get(timelineType ?? WI.TimelineRecord.Type.Script);
            let recordAddedListener = timeline.addEventListener(WI.Timeline.Event.RecordAdded, function(recordAddedEvent) {
                let {record} = recordAddedEvent.data;
                if (eventType && record.eventType !== eventType)
                    return;
                timeline.removeEventListener(WI.Timeline.Event.RecordAdded, recordAddedListener);
                callback();
            });
        });
    }

    InspectorTest.TimelineEvent.captureTimelineWithMarker = function({expression, checkMarker, silent = false}) {
        return captureTimeline(expression, silent, function(callback) {
            let recording = WI.timelineManager.activeRecording;
            let markerAddedListener = recording.addEventListener(WI.TimelineRecording.Event.MarkerAdded, function(markerAddedEvent) {
                let {marker} = markerAddedEvent.data;
                if (checkMarker && !checkMarker(marker))
                    return;
                recording.removeEventListener(WI.TimelineRecording.Event.MarkerAdded, markerAddedListener);
                callback();
            });
        });
    }
});
