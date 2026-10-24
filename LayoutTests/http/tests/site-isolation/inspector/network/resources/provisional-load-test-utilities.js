TestPage.registerInitializer(function() {

// Each phase of a test case shares one deadline, so a failing run prints FAIL lines well before the harness timeout.
window.phaseTimeout = 5000;

window.startPhase = function() {
    let deadline = Date.now() + phaseTimeout;
    return () => Math.max(0, deadline - Date.now());
};

window.childFrameWithQuery = function(query) {
    return Array.from(WI.networkManager.mainFrame.childFrameCollection).find((frame) => frame.mainResource.url.endsWith(`?${query}`));
};

function isSettled(resource) {
    return resource.finished || resource.failed;
}

// Evaluates `expression`, which loads resources/iframe-with-script.html?<query> in an iframe, and checks the document and script rows.
// Resolves to the frame, or to null after a FAIL line when the load does not settle within one phase.
window.loadAndExpectDocumentAndScriptRows = async function(recorder, expression, query) {
    let remaining = startPhase();
    InspectorTest.evaluateInPage(expression);
    let frame = await pollUntil(() => childFrameWithQuery(query), remaining());
    InspectorTest.expectThat(!!frame, "Child frame should show the cross-origin document.");
    if (!frame)
        return null;

    let settled = await pollUntil(() => {
        let script = Array.from(frame.resourceCollection).find((resource) => resource.url.includes(`iframe-script.js?${query}`));
        return isSettled(frame.mainResource) && script && isSettled(script);
    }, remaining());
    InspectorTest.expectThat(!!settled, "Document and script should finish loading.");
    if (!settled)
        return null;

    await expectSingleFinishedDocument(recorder, frame, "Document");
    await expectSingleFinishedSubresource(recorder, frame, `iframe-script.js?${query}`, "Script");
    return frame;
};

// Evaluates `expression`, which starts a cross-site iframe navigation, and holds that load paused, as provisional-frame-target-pausing.html does.
// Resolves to the provisional frame target, or to null after a FAIL line.
window.holdProvisionalLoad = async function(expression) {
    let remaining = startPhase();
    let originalResumeIfPaused = WI.Target.prototype._resumeIfPaused;
    WI.Target.prototype._resumeIfPaused = function() { };
    InspectorTest.evaluateInPage(expression);
    let target = await pollUntil(() => WI.targetManager.targets.find((target) => target.type === WI.TargetType.Frame && target.isProvisional), remaining());
    WI.Target.prototype._resumeIfPaused = originalResumeIfPaused;
    InspectorTest.expectThat(target?.isPaused, "Provisional frame target should be paused.");
    return target?.isPaused ? target : null;
};

});
