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

    // Fails the test case if the expression throws, so that a missing function shows up as a failure, not a timeout.
    FrameTimelineTest.evaluateInTarget = async function(target, expression) {
        let response = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test"});
        if (response.wasThrown)
            throw new Error(`Evaluating "${expression}" threw: ${response.result.description}`);
        return response;
    };
});
