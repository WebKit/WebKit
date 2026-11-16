// The frame loses its inspector connection during breakpoint evaluation, so it reports here by postMessage.

let messagesFromFrame = [];

window.addEventListener("message", (event) => {
    messagesFromFrame.push(event.data);
});

function messagesFromFrameJoined()
{
    return messagesFromFrame.join("\n");
}

function runFunctionWithBreakpointInFrame()
{
    frames[0].postMessage("run", "*");
}

TestPage.registerInitializer(function() {

// Works only once per test, since the frame target is disconnected afterward.
window.setBreakpointInFrameScript = async function setBreakpointInFrameScript(target, options) {
    let script = WI.debuggerManager.dataForTarget(target).scripts.find((script) => script.url && script.url.includes("breakpoint-detach-frame-script.js"));
    InspectorTest.expectNotNull(script, "Should find the frame's script.");
    if (!script)
        return;

    await target.DebuggerAgent.setBreakpointsActive.invoke({active: true});
    await target.DebuggerAgent.setBreakpoint.invoke({
        // The report() line in breakpoint-detach-frame-script.js.
        location: {scriptId: script.id, lineNumber: 4, columnNumber: 0},
        options,
    });
};

});
