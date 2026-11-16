TestPage.registerInitializer(function() {

// Evaluate the expression in the target's main world. Resolves after the evaluation finishes, which
// for code that pauses means after the debugger resumes.
window.evaluateInFrameTarget = function evaluateInFrameTarget(target, expression) {
    return target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test", returnByValue: true});
};

// Add an event breakpoint through WI.domDebuggerManager, which sends it to every target that has the
// DOMDebugger domain.
window.addEventBreakpoint = async function addEventBreakpoint(type, options = {}) {
    InspectorTest.log(`Adding "${type + (options.eventName ? ":" + options.eventName : "")}" Event Breakpoint...`);

    let breakpoint = new WI.EventBreakpoint(type, options);
    let breakpointAddedPromise = WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.EventBreakpointAdded);
    WI.domDebuggerManager.addEventBreakpoint(breakpoint);
    await breakpointAddedPromise;
    return breakpoint;
};

// Add a URL breakpoint through WI.domDebuggerManager. An empty URL makes the All Requests breakpoint.
window.addURLBreakpoint = async function addURLBreakpoint(type, url) {
    if (url)
        InspectorTest.log(`Adding "${type}:${url}" URL Breakpoint...`);
    else
        InspectorTest.log(`Adding All Requests URL Breakpoint...`);

    let breakpoint = new WI.URLBreakpoint(type, url);
    let breakpointAddedPromise = WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.URLBreakpointAdded);
    WI.domDebuggerManager.addURLBreakpoint(breakpoint);
    await breakpointAddedPromise;
    return breakpoint;
};

// Evaluate the expression in the target, wait for the next pause, check that it happened in that
// target for the expected reason, then resume. Returns the pause data.
window.expectPauseInFrameTarget = async function expectPauseInFrameTarget(target, expression, {pauseReason, eventName, breakpointURL, functionName}) {
    let pausedPromise = WI.debuggerManager.awaitEvent(WI.DebuggerManager.Event.Paused);
    let evaluatePromise = evaluateInFrameTarget(target, expression);
    await pausedPromise;

    let activeCallFrame = WI.debuggerManager.activeCallFrame;
    InspectorTest.expectThat(activeCallFrame.target === target, "Should pause in the frame target that ran the code.");

    let targetData = WI.debuggerManager.dataForTarget(activeCallFrame.target);
    let pauseData = targetData.pauseData;
    InspectorTest.expectEqual(targetData.pauseReason, pauseReason, `Pause reason should be "${pauseReason}".`);
    if (eventName !== undefined)
        InspectorTest.expectEqual(pauseData?.eventName, eventName, `Pause data eventName should be "${eventName}".`);
    if (breakpointURL !== undefined)
        InspectorTest.expectEqual(pauseData?.breakpointURL, breakpointURL, `Pause data breakpointURL should be "${breakpointURL}".`);
    if (functionName !== undefined)
        InspectorTest.expectEqual(activeCallFrame.functionName, functionName, `Top call frame should be "${functionName}".`);

    await WI.debuggerManager.resume();
    await evaluatePromise;
    return pauseData;
};

// Run the async work, resuming every pause that happens meanwhile. Returns the paused targets.
window.collectPausesDuring = async function collectPausesDuring(work) {
    let pausedTargets = [];
    let listener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
        pausedTargets.push(WI.debuggerManager.activeCallFrame.target);
        WI.debuggerManager.resume();
    });

    try {
        await work();
    } finally {
        WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);
    }

    return pausedTargets;
};

// Remove every event and URL breakpoint, and wait until every target has processed the removals.
// An "All Timeouts" breakpoint left behind would catch the harness's own setTimeout and hang.
window.removeAllDOMDebuggerBreakpoints = async function removeAllDOMDebuggerBreakpoints() {
    if (WI.debuggerManager.paused)
        await WI.debuggerManager.resume();

    WI.domDebuggerManager.allAnimationFramesBreakpoint?.remove();
    WI.domDebuggerManager.allIntervalsBreakpoint?.remove();
    WI.domDebuggerManager.allListenersBreakpoint?.remove();
    WI.domDebuggerManager.allTimeoutsBreakpoint?.remove();
    WI.domDebuggerManager.allRequestsBreakpoint?.remove();

    for (let breakpoint of WI.domDebuggerManager.listenerBreakpoints.slice())
        breakpoint.remove();

    for (let breakpoint of WI.domDebuggerManager.urlBreakpoints.slice())
        breakpoint.remove();

    let targets = WI.targets.filter((target) => target.hasDomain("DOMDebugger") && target.hasDomain("Runtime"));
    await Promise.all(targets.map((target) => target.RuntimeAgent.evaluate.invoke({expression: "0", objectGroup: "test"})));
};

});
