TestPage.registerInitializer(function() {

const deadlineDuration = 10000;

window.withDeadline = function withDeadline(promise, description) {
    let timer = 0;
    let deadline = new Promise((resolve, reject) => {
        timer = setTimeout(() => {
            InspectorTest.fail(`Timed out: ${description}`);
            reject(new Error(`Timed out: ${description}`));
        }, deadlineDuration);
    });
    return Promise.race([promise, deadline]).finally(() => clearTimeout(timer));
};

window.evaluateInTarget = function evaluateInTarget(target, expression) {
    if (!target)
        return withDeadline(InspectorTest.evaluateInPage(expression), `evaluate in page: ${expression}`);
    return withDeadline(evaluateInFrameTarget(target, expression), `evaluate in frame: ${expression}`);
};

window.flushCommands = async function flushCommands(...targets) {
    await evaluateInTarget(null, "0");
    for (let target of targets) {
        if (target && WI.targets.includes(target))
            await evaluateInTarget(target, "0");
    }
};

window.frameTargetDocument = async function frameTargetDocument(target) {
    while (!WI.domManager.frameTargetDocumentForTarget(target))
        await withDeadline(WI.domManager.awaitEvent(WI.DOMManager.Event.FrameDocumentAvailable), "frame document");
    return WI.domManager.frameTargetDocumentForTarget(target);
};

window.nodeForExpressionInTarget = async function nodeForExpressionInTarget(target, expression) {
    let {result} = await withDeadline(target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test"}), `evaluate node: ${expression}`);
    let remoteObject = WI.RemoteObject.fromPayload(result, target);
    let nodeId = await withDeadline(new Promise((resolve) => remoteObject.pushNodeToFrontend(resolve)), `push node: ${expression}`);
    return WI.domManager.nodeForId(nodeId);
};

window.eventListenerForNode = async function eventListenerForNode(node, handlerName, {includeAncestors} = {}) {
    let {listeners} = await withDeadline(node.getEventListeners({includeAncestors: !!includeAncestors}), `getEventListeners for ${handlerName}`);
    return listeners.find((listener) => listener.handlerName === handlerName) || null;
};

window.breakpointForListener = function breakpointForListener(listener) {
    return WI.domManager.breakpointForEventListenerId(listener.eventListenerId, listener.owningTarget);
};

window.setListenerBreakpoint = async function setListenerBreakpoint(listener) {
    InspectorTest.log(`Setting breakpoint on "${listener.handlerName}"...`);
    WI.domManager.setBreakpointForEventListener(listener);
    await flushCommands(listener.owningTarget);
    return breakpointForListener(listener);
};

window.removeListenerBreakpoint = async function removeListenerBreakpoint(listener) {
    InspectorTest.log(`Removing breakpoint on "${listener.handlerName}"...`);
    WI.domManager.removeBreakpointForEventListener(listener);
    await flushCommands(listener.owningTarget);
};

window.removeAllListenerBreakpoints = async function removeAllListenerBreakpoints() {
    if (WI.debuggerManager.paused)
        await WI.debuggerManager.resume();

    let targets = new Set;
    for (let breakpoint of WI.domManager.eventListenerBreakpoints) {
        targets.add(breakpoint.eventListener.owningTarget);
        WI.domManager.removeBreakpointForEventListener(breakpoint.eventListener);
    }
    await flushCommands(...targets);
};

// Also counts backend pauses, since the frontend silently resumes a pause without call frames it knows.
window.captureListenerPausesDuring = async function captureListenerPausesDuring(work) {
    let pauses = [];
    let backendPauseCount = 0;
    let debuggerDidPause = WI.debuggerManager.debuggerDidPause;
    WI.debuggerManager.debuggerDidPause = function(...args) {
        ++backendPauseCount;
        return debuggerDidPause.apply(this, args);
    };
    let listener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
        try {
            let callFrame = WI.debuggerManager.activeCallFrame;
            let target = callFrame.target;
            let {pauseReason, pauseData} = WI.debuggerManager.dataForTarget(target);
            let resolvedBreakpoint = null;
            if (pauseData?.eventListenerId)
                resolvedBreakpoint = WI.domManager.breakpointForEventListenerId(pauseData.eventListenerId, pauseData.frameTarget ? target : null);
            pauses.push({target, pauseReason, pauseData, functionName: callFrame.functionName, resolvedBreakpoint});
        } finally {
            WI.debuggerManager.resume();
        }
    });

    try {
        await work();
    } finally {
        WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);
        delete WI.debuggerManager.debuggerDidPause;
    }
    return {pauses, backendPauseCount};
};

window.expectListenerPause = async function expectListenerPause(target, expression, listener, {functionName, pauseTarget} = {}) {
    let {pauses, backendPauseCount} = await captureListenerPausesDuring(() => evaluateInTarget(target, expression));
    InspectorTest.expectEqual(backendPauseCount, 1, "Should pause once.");

    let pause = pauses[0];
    if (pauseTarget)
        InspectorTest.expectThat(pause?.target === pauseTarget, "Should pause in the frame target that ran the listener.");
    InspectorTest.expectEqual(pause?.pauseReason, WI.DebuggerManager.PauseReason.Listener, `Pause reason should be "${WI.DebuggerManager.PauseReason.Listener}".`);
    InspectorTest.expectEqual(pause?.functionName, functionName, `Top call frame should be "${functionName}".`);
    InspectorTest.expectEqual(pause?.pauseData?.eventListenerId, listener.eventListenerId, "Pause data should carry the listener's id.");
    let frameTargetFlag = !!listener.owningTarget;
    InspectorTest.expectEqual(!!pause?.pauseData?.frameTarget, frameTargetFlag, `Pause data frameTarget should be ${frameTargetFlag}.`);
    InspectorTest.expectThat(!!pause?.resolvedBreakpoint && pause.resolvedBreakpoint === breakpointForListener(listener), "Pause data should resolve to the breakpoint of that listener.");
};

window.expectNoListenerPause = async function expectNoListenerPause(target, expression, message) {
    let {backendPauseCount} = await captureListenerPausesDuring(() => evaluateInTarget(target, expression));
    InspectorTest.expectEqual(backendPauseCount, 0, message);
};

window.countPausesDuring = async function countPausesDuring(target, expression) {
    let {backendPauseCount} = await captureListenerPausesDuring(() => evaluateInTarget(target, expression));
    return backendPauseCount;
};

window.addListenerBreakpointTestCase = function addListenerBreakpointTestCase(suite, {name, description, test}) {
    suite.addTestCase({
        name,
        description,
        test,
        async teardown() {
            await removeAllListenerBreakpoints();
            await removeAllDOMDebuggerBreakpoints();
        },
    });
};

});
