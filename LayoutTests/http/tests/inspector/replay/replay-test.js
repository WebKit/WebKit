InspectorTestProxy.registerInitializer(function() {

InspectorTest.Replay = {};

InspectorTest.Replay.runSingleSegmentRefTest = function(stateComparator)
{
    var stateDuringCapturing = null;
    var stateDuringReplaying = null;

    var ignoreCacheOnReload = true;
    InspectorTest.reloadPage(ignoreCacheOnReload)
    .then(function() {
        return new Promise(function waitForMainResourceBeforeStarting(resolve, reject) {
            InspectorTest.eventDispatcher.addEventListener(InspectorTest.EventDispatcher.Event.TestPageDidLoad, resolve);
            InspectorTest.log("Waiting for test page to finish its initial load...");
        });
    })
    .then(function() {
        InspectorTest.log("Test page initial load done.");
        return new Promise(function startCapturing(resolve, reject) {
            InspectorTest.log("Waiting for capturing to start...");
            WebInspector.replayManager.startCapturing();
            WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStarted, resolve);
        });
    })
    .then(function() {
        InspectorTest.log("Capturing has started.");
        return new Promise(function waitToCaptureInitialNavigation(resolve, reject) {
            InspectorTest.log("Waiting to capture initial navigation...");
            InspectorTest.eventDispatcher.addEventListener(InspectorTest.EventDispatcher.Event.TestPageDidLoad, resolve);
        });
    })
    .then(function() {
        InspectorTest.log("Initial navigation captured. Dumping state....");
        return RuntimeAgent.evaluate.promise("dumpNondeterministicState()");
    })
    .then(function(payload) {
        stateDuringCapturing = payload.value;
        return new Promise(function stopCapturing(resolve, reject) {
            WebInspector.replayManager.stopCapturing();
            WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStopped, resolve);
        });
    })
    .then(function() {
        InspectorTest.log("Capture stopped, now starting replay to completion...")
        return new Promise(function startPlayback(resolve, reject) {
            WebInspector.replayManager.replayToCompletion();
            WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.PlaybackStarted, resolve);
        });
    })
    .then(function() {
        InspectorTest.log("Playback has started.");
        return new Promise(function waitForMainResourceWhenReplaying(resolve, reject) {
            InspectorTest.log("Waiting to replay initial navigation...");
            InspectorTest.eventDispatcher.addEventListener(InspectorTest.EventDispatcher.Event.TestPageDidLoad, resolve);
        });
    })
    .then(function() {
        InspectorTest.log("Initial navigation replayed. Dumping state...");
        return RuntimeAgent.evaluate.promise("dumpNondeterministicState()");
    })
    .then(function(payload) {
        stateDuringReplaying = payload.value;
        return new Promise(function waitForReplayingToFinish(resolve, reject) {
            WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.PlaybackFinished, resolve);
        });
    })
    .then(function() {
        var statesEqual = stateDuringCapturing === stateDuringReplaying;
        InspectorTest.expectThat(statesEqual, "Nondeterministic state should not differ during capture and replay.");
        if (!statesEqual) {
            InspectorTest.log("State during capturing: " + stateDuringCapturing);
            InspectorTest.log("State during replaying: " + stateDuringReplaying);
        }
        InspectorTest.completeTest();
    });
};

});