TestPage.registerInitializer(function() {

// Helpers for tests that set DOM breakpoints on nodes of the cross-origin iframe that loads resources/dom-breakpoint-frame.html,
// and of the same-process frame nested in it. The setup test cases set these two targets.
window.crossOriginTarget = null;
window.nestedTarget = null;

let breakpoints = [];

window.nodeForExpression = async function nodeForExpression(target, expression) {
    let {result} = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test"});
    let remoteObject = WI.RemoteObject.fromPayload(result, target);
    let nodeId = await new Promise((resolve) => remoteObject.pushNodeToFrontend(resolve));
    return WI.domManager.nodeForId(nodeId);
};

window.nodeForElementId = function nodeForElementId(id) {
    return nodeForExpression(crossOriginTarget, `document.getElementById("${id}")`);
};

window.pageNodeForElementId = async function pageNodeForElementId(id) {
    let pageDocument = await WI.domManager.requestDocument();
    let nodeId = await new Promise((resolve) => pageDocument.querySelector(`#${id}`, resolve));
    let node = WI.domManager.nodeForId(nodeId);
    InspectorTest.assert(node && !node.owningTarget, "Node should belong to the page target's tree.");
    return node;
};

// Removed in the teardown of each test case.
window.trackDOMBreakpoint = function trackDOMBreakpoint(breakpoint) {
    breakpoints.push(breakpoint);
};

window.addDOMBreakpointForNode = function addDOMBreakpointForNode(node, type, options) {
    let breakpoint = new WI.DOMBreakpoint(node, type, options);
    trackDOMBreakpoint(breakpoint);
    WI.domDebuggerManager.addDOMBreakpoint(breakpoint);
    return breakpoint;
};

window.addDOMBreakpoint = async function addDOMBreakpoint(elementId, type, options) {
    InspectorTest.log(`Adding "${type}" DOM Breakpoint on #${elementId}...`);

    let node = await nodeForElementId(elementId);
    InspectorTest.assert(node?.owningTarget === crossOriginTarget, "Node should belong to the cross-origin frame target.");

    let breakpoint = addDOMBreakpointForNode(node, type, options);

    // Commands to one target are handled in order, so this makes sure the breakpoint is set.
    await evaluateInFrameTarget(crossOriginTarget, "0");
    return breakpoint;
};

window.trackDOMBreakpointRemoval = function trackDOMBreakpointRemoval(breakpoint) {
    let removal = {removed: false};
    WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, (event) => {
        if (event.data.breakpoint === breakpoint)
            removal.removed = true;
    }, removal);
    return removal;
};

window.removeDOMBreakpoint = function removeDOMBreakpoint(breakpoint) {
    breakpoints.remove(breakpoint);
    breakpoint.remove();
};

function evaluate(expression, {inPage, inNestedFrame} = {}) {
    if (inPage)
        return InspectorTest.evaluateInPage(expression);
    return evaluateInFrameTarget(inNestedFrame ? nestedTarget : crossOriginTarget, expression);
}

// Records every pause until the work finishes. An evaluation finishes after the debugger resumes,
// so this never waits for a pause that does not happen. Also counts the pauses the backend sends,
// since the frontend resumes a pause without call frames it knows, and never shows it.
async function capturePausesDuring(work) {
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
            let {pauseReason, pauseData} = WI.debuggerManager.dataForTarget(callFrame.target);
            pauses.push({
                target: callFrame.target,
                reason: pauseReason,
                data: pauseData,
                functionName: callFrame.functionName,
                // The Sources tab shows the pause reason for this breakpoint.
                breakpoint: WI.domDebuggerManager.domBreakpointForPauseData(pauseData, callFrame.target),
            });
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
}

window.expectDOMBreakpointPause = async function expectDOMBreakpointPause(expression, breakpoint, {functionName, insertion, targetNode, inPage, inNestedFrame}) {
    let expectedNodeId = breakpoint.domNode.backendNodeId;
    let expectedTargetNodeId = targetNode?.backendNodeId;
    let expectedTarget = breakpoint.domNode.owningTarget;

    let {pauses, backendPauseCount} = await capturePausesDuring(() => evaluate(expression, {inPage, inNestedFrame}));
    InspectorTest.expectEqual(backendPauseCount, 1, "Should pause exactly once.");
    let pause = pauses[0];
    InspectorTest.assert(pauses.length === backendPauseCount, "The frontend should show every pause.");
    if (!pause)
        return;

    if (!expectedTarget)
        InspectorTest.expectThat(pause.target === InspectorTest.mainFrameTarget, "Should pause in the main frame's target.");
    else if (expectedTarget === nestedTarget)
        InspectorTest.expectThat(pause.target === nestedTarget, "Should pause in the nested frame's target.");
    else
        InspectorTest.expectThat(pause.target === crossOriginTarget, "Should pause in the cross-origin frame target.");
    InspectorTest.expectEqual(pause.reason, WI.DebuggerManager.PauseReason.DOM, "Pause reason should be DOM.");
    InspectorTest.expectEqual(pause.data?.type, breakpoint.type, `Pause type should be "${breakpoint.type}".`);
    InspectorTest.expectEqual(pause.data?.nodeId, expectedNodeId, `Pause nodeId should be the breakpoint node, in the ${expectedTarget ? "frame" : "page"} target's id space.`);
    InspectorTest.expectEqual(pause.data?.targetNodeId, expectedTargetNodeId, expectedTargetNodeId ? "Pause targetNodeId should be the removed node." : "Pause should have no targetNodeId.");
    InspectorTest.expectEqual(pause.data?.insertion, insertion, insertion ? "Pause should be for an insertion." : "Pause should not be for an insertion.");
    InspectorTest.expectEqual(pause.functionName, functionName, `Top call frame should be "${functionName}".`);
    InspectorTest.expectEqual(pause.breakpoint, breakpoint, "Pause data should lead to the breakpoint.");
};

window.expectNoPause = async function expectNoPause(expression, message, {inPage, inNestedFrame} = {}) {
    let {backendPauseCount} = await capturePausesDuring(() => evaluate(expression, {inPage, inNestedFrame}));
    InspectorTest.expectEqual(backendPauseCount, 0, message);
};

window.addDOMBreakpointTestCase = function addDOMBreakpointTestCase(suite, {name, description, test}) {
    suite.addTestCase({
        name,
        description,
        test,
        async teardown() {
            for (let breakpoint of breakpoints)
                breakpoint.remove();
            breakpoints = [];

            await InspectorTest.evaluateInPage("0");
            if (crossOriginTarget)
                await evaluateInFrameTarget(crossOriginTarget, "0");
            if (nestedTarget && WI.targets.includes(nestedTarget))
                await evaluateInFrameTarget(nestedTarget, "0");
        },
    });
};

window.addDOMBreakpointSetupTestCase = function addDOMBreakpointSetupTestCase(suite, {withNestedFrame} = {}) {
    addDOMBreakpointTestCase(suite, {
        name: `${suite.name}.Setup`,
        description: "Find the cross-origin frame target and splice its document under the iframe element, as the Elements tab does.",
        async test() {
            crossOriginTarget = await frameTargetForURLContaining("dom-breakpoint-frame.html");
            InspectorTest.expectNotNull(crossOriginTarget, "Should find the cross-origin frame target.");

            while (!WI.domManager.frameTargetDocumentForTarget(crossOriginTarget))
                await WI.domManager.awaitEvent(WI.DOMManager.Event.FrameDocumentAvailable);

            let pageDocument = await WI.domManager.requestDocument();
            await new Promise((resolve) => pageDocument.querySelector("iframe", resolve));

            let frameDocument = WI.domManager.frameTargetDocumentForTarget(crossOriginTarget);
            InspectorTest.expectEqual(frameDocument.parentNode?.nodeName(), "IFRAME", "Frame document should be spliced under the iframe element.");

            if (!withNestedFrame)
                return;

            // The same-process frame nested in the cross-origin iframe has its own frame target.
            nestedTarget = await waitForFrameTargetForURLContaining("about:srcdoc");
            InspectorTest.expectThat(nestedTarget && nestedTarget !== crossOriginTarget, "Should find the nested frame's target.");
            while (!WI.domManager.frameTargetDocumentForTarget(nestedTarget))
                await WI.domManager.awaitEvent(WI.DOMManager.Event.FrameDocumentAvailable);
            InspectorTest.assert(await waitForExpressionInTarget(nestedTarget, "!!window.modifyOwnAttribute", 50), "Nested frame should load.");
        }
    });
};

});
