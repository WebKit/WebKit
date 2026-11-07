TestPage.registerInitializer(() => {
    // Frame targets in other processes can start tracking after capturing has become active, since capturing becomes
    // active on the first start reported by any target. So tests wait for the start reports of the targets they use.
    let trackingEvents = [];
    let trackingEventListeners = [];
    for (let functionName of ["capturingStarted", "capturingStopped"]) {
        let original = WI.timelineManager[functionName];
        WI.timelineManager[functionName] = function(target, domain, ...rest) {
            let result = original.call(this, target, domain, ...rest);

            trackingEvents.push({functionName, target, domain});
            for (let listener of trackingEventListeners.slice()) {
                if (listener.functionName === functionName && listener.target === target && listener.domain === domain) {
                    trackingEventListeners.remove(listener);
                    listener.resolve();
                }
            }
            return result;
        };
    }

    // Layout timeline records do not know which target sent them, so keep the target and type of every record
    // payload, nested ones included.
    let recordPayloads = [];
    let recordPayloadListeners = [];
    let originalEventRecorded = WI.timelineManager.eventRecorded;
    WI.timelineManager.eventRecorded = function(target, recordPayload) {
        let payloads = [recordPayload];
        while (payloads.length) {
            let payload = payloads.pop();
            recordPayloads.push({target, type: payload.type});
            for (let listener of recordPayloadListeners.slice()) {
                if (listener.target === target && listener.type === payload.type) {
                    recordPayloadListeners.remove(listener);
                    listener.resolve();
                }
            }
            if (payload.children)
                payloads.push(...payload.children);
        }
        return originalEventRecorded.call(this, target, recordPayload);
    };

    window.FrameTimelineTest = {};

    // Resolves when the target reports that the domain started (or stopped) tracking, including if it already did
    // since capturing last started.
    FrameTimelineTest.awaitTrackingEvent = function(functionName, target, domain = "Timeline") {
        if (trackingEvents.some((event) => event.functionName === functionName && event.target === target && event.domain === domain))
            return Promise.resolve();

        return new Promise((resolve) => {
            trackingEventListeners.push({functionName, target, domain, resolve});
        });
    };

    FrameTimelineTest.clearTrackingEvents = function() {
        trackingEvents = [];
    };

    FrameTimelineTest.didReportTrackingEvent = function(functionName, target, domain = "Timeline") {
        return trackingEvents.some((event) => event.functionName === functionName && event.target === target && event.domain === domain);
    };

    // Resolves when the target sends a record payload of the given type.
    FrameTimelineTest.awaitRecordPayload = function(target, type) {
        return new Promise((resolve) => {
            recordPayloadListeners.push({target, type, resolve});
        });
    };

    FrameTimelineTest.clearRecordPayloads = function() {
        recordPayloads = [];
    };

    // Returns the types of the record payloads from the target since they were last cleared.
    FrameTimelineTest.recordPayloadTypes = function(target) {
        return recordPayloads.filter((payload) => payload.target === target).map((payload) => payload.type);
    };

    FrameTimelineTest.awaitCapturingState = function(capturingState) {
        return new Promise((resolve) => {
            let listener = WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStateChanged, (event) => {
                if (WI.timelineManager.capturingState !== capturingState)
                    return;

                WI.timelineManager.removeEventListener(WI.TimelineManager.Event.CapturingStateChanged, listener);
                resolve(event.data);
            });
        });
    };

    // Starts a new recording, and resolves once each of the given targets has started its Timeline domain.
    FrameTimelineTest.startCapturing = async function(targets) {
        FrameTimelineTest.clearTrackingEvents();
        FrameTimelineTest.clearRecordPayloads();

        let activePromise = FrameTimelineTest.awaitCapturingState(WI.TimelineManager.CapturingState.Active);
        const newRecording = true;
        WI.timelineManager.startCapturing(newRecording);
        await activePromise;

        await Promise.all(targets.map((target) => FrameTimelineTest.awaitTrackingEvent("capturingStarted", target)));
    };

    // Resolves with the recording's end time once capturing is inactive.
    FrameTimelineTest.stopCapturing = async function() {
        let inactivePromise = FrameTimelineTest.awaitCapturingState(WI.TimelineManager.CapturingState.Inactive);
        WI.timelineManager.stopCapturing();
        return (await inactivePromise).endTime;
    };

    // Resolves with the first record of the given type from the target in the active recording's Script timeline.
    FrameTimelineTest.awaitScriptRecord = function(target, eventType) {
        let scriptTimeline = WI.timelineManager.activeRecording.timelines.get(WI.TimelineRecord.Type.Script);
        return new Promise((resolve) => {
            let listener = scriptTimeline.addEventListener(WI.Timeline.Event.RecordAdded, (event) => {
                let {record} = event.data;
                if (record.target !== target || record.eventType !== eventType)
                    return;

                scriptTimeline.removeEventListener(WI.Timeline.Event.RecordAdded, listener);
                resolve(record);
            });
        });
    };

    // Resolves with the first record of the given type from the target in the active recording's Layout timeline.
    FrameTimelineTest.awaitLayoutRecord = function(target, eventType) {
        let layoutTimeline = WI.timelineManager.activeRecording.timelines.get(WI.TimelineRecord.Type.Layout);
        return new Promise((resolve) => {
            let listener = layoutTimeline.addEventListener(WI.Timeline.Event.RecordAdded, (event) => {
                let {record} = event.data;
                if (record.target !== target || record.eventType !== eventType)
                    return;

                layoutTimeline.removeEventListener(WI.Timeline.Event.RecordAdded, listener);
                resolve(record);
            });
        });
    };

    // Resolves once the frontend has the frame target's document, which a frame target needs before it can send
    // node IDs.
    FrameTimelineTest.awaitFrameDocument = async function(target) {
        if (WI.domManager.frameTargetDocumentForTarget(target))
            return;

        await new Promise((resolve) => {
            let listener = WI.domManager.addEventListener(WI.DOMManager.Event.FrameDocumentAvailable, (event) => {
                if (event.data.target !== target)
                    return;

                WI.domManager.removeEventListener(WI.DOMManager.Event.FrameDocumentAvailable, listener);
                resolve();
            });
        });
    };

    // Fails the test case if the expression throws, so that a missing function shows up as a failure, not a timeout.
    FrameTimelineTest.evaluateInTarget = async function(target, expression) {
        let response = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test"});
        if (response.wasThrown)
            throw new Error(`Evaluating "${expression}" threw: ${response.result.description}`);
        return response;
    };
});
