TestPage.registerInitializer(() => {
    window.FrameScriptProfilerTest = {};

    let scriptProfilerEvents = [];
    let originalScriptProfilerTrackingUpdated = WI.timelineManager.scriptProfilerTrackingUpdated;
    WI.timelineManager.scriptProfilerTrackingUpdated = function(target, event) {
        scriptProfilerEvents.push({target, event});
        return originalScriptProfilerTrackingUpdated.call(this, target, event);
    };

    FrameScriptProfilerTest.clearScriptProfilerEvents = function() {
        scriptProfilerEvents = [];
    };

    FrameScriptProfilerTest.scriptProfilerEventsForTarget = function(target) {
        return scriptProfilerEvents.filter((entry) => entry.target === target).map((entry) => entry.event);
    };

    // Resolves once each of the given targets has started both its Timeline and ScriptProfiler domains.
    FrameScriptProfilerTest.startCapturing = async function(targets) {
        FrameScriptProfilerTest.clearScriptProfilerEvents();
        await FrameTimelineTest.startCapturing(targets);
        await Promise.all(targets.map((target) => FrameTimelineTest.awaitTrackingEvent("capturingStarted", target, "ScriptProfiler")));
    };

    FrameScriptProfilerTest.runInTimer = async function(target, functionName) {
        let response = await target.RuntimeAgent.evaluate.invoke({expression: `runInTimer("${functionName}")`, objectGroup: "test", awaitPromise: true});
        if (response.wasThrown)
            throw new Error(`Running ${functionName} threw: ${response.result.description}`);
    };

    FrameScriptProfilerTest.processIdentifier = function(target) {
        return target.identifier.split("-").pop();
    };

    // Tracks with ScriptProfiler on the target alone, outside of a Timeline recording, while running the callback,
    // and resolves with the samples that the target reported.
    FrameScriptProfilerTest.trackSamplesOutsideRecording = async function(target, callback) {
        let timelineManager = WI.timelineManager;
        let {scriptProfilerTrackingStarted, scriptProfilerTrackingUpdated, scriptProfilerTrackingCompleted} = timelineManager;

        let samplesPromise = new Promise((resolve) => {
            timelineManager.scriptProfilerTrackingCompleted = function(trackingTarget, timestamp, samples) {
                if (trackingTarget === target)
                    resolve(samples);
            };
        });
        timelineManager.scriptProfilerTrackingStarted = function() { };
        timelineManager.scriptProfilerTrackingUpdated = function() { };

        try {
            await target.ScriptProfilerAgent.startTracking(true);
            await callback();
            await target.ScriptProfilerAgent.stopTracking();
            return await samplesPromise;
        } finally {
            Object.assign(timelineManager, {scriptProfilerTrackingStarted, scriptProfilerTrackingUpdated, scriptProfilerTrackingCompleted});
        }
    };

    // The names of the functions in the given protocol samples.
    FrameScriptProfilerTest.functionNamesInSamples = function(samples) {
        let names = new Set;
        for (let stackTrace of samples?.stackTraces || []) {
            for (let stackFrame of stackTrace.stackFrames) {
                if (stackFrame.name)
                    names.add(stackFrame.name);
            }
        }
        return names;
    };

    // The names of the sampled functions in the recording, for each target that reported samples.
    FrameScriptProfilerTest.sampledFunctionNamesByTarget = function(recording) {
        let scriptTimeline = recording.timelines.get(WI.TimelineRecord.Type.Script);
        return new Map(scriptTimeline.targets.map((target) => [target, FrameScriptProfilerTest.sampledFunctionNames(recording, target)]));
    };

    // The sampling profiler's thread can miss a whole busy loop, so this runs the attempt again, up to maxAttempts times,
    // while an expected function has no samples on any target. It never runs again after a sample on the wrong target, so
    // the final checks still see it. The attempt must not log anything. It resolves with `functionNamesByTarget`,
    // `expectedFunctionNames`, and `targetForFunctionName`, which maps each function it ran to the only target allowed to
    // sample it.
    FrameScriptProfilerTest.repeatWhileUnsampled = async function(attempt, maxAttempts = 3) {
        let result;
        for (let i = 0; i < maxAttempts; ++i) {
            result = await attempt();

            let sampledFunctionNames = new Set;
            let hasSampleOnWrongTarget = false;
            for (let [target, functionNames] of result.functionNamesByTarget) {
                for (let functionName of functionNames) {
                    sampledFunctionNames.add(functionName);

                    let expectedTarget = result.targetForFunctionName.get(functionName);
                    if (expectedTarget && expectedTarget !== target)
                        hasSampleOnWrongTarget = true;
                }
            }
            if (hasSampleOnWrongTarget || result.expectedFunctionNames.every((functionName) => sampledFunctionNames.has(functionName)))
                break;
        }
        return result;
    };

    // The number of samples that the target reported in the recording.
    FrameScriptProfilerTest.sampleCount = function(recording, target) {
        let scriptTimeline = recording.timelines.get(WI.TimelineRecord.Type.Script);
        return scriptTimeline.callingContextTree(target, WI.CallingContextTree.Type.TopDown)?.totalNumberOfSamples ?? 0;
    };

    // The names of the functions in the samples that the target reported in the recording.
    FrameScriptProfilerTest.sampledFunctionNames = function(recording, target) {
        let names = new Set;
        let scriptTimeline = recording.timelines.get(WI.TimelineRecord.Type.Script);
        let callingContextTree = scriptTimeline.callingContextTree(target, WI.CallingContextTree.Type.TopDown);
        callingContextTree?.forEachNode((node) => {
            if (node.name)
                names.add(node.name);
        });
        return names;
    };
});
