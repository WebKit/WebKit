// The frames these tests drive lose their inspector connection in the middle of breakpoint
// evaluation, so they report what happened by postMessage to this page instead. The frontend reads
// it back over the page target, whose own connection is untouched.

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

// Both tests break on the same line of the same frame script, and each can only do it once: the
// frame target's connection is gone afterwards, so no further breakpoint can be set on it.
window.setBreakpointInFrameScript = async function setBreakpointInFrameScript(target, options) {
    let script = WI.debuggerManager.dataForTarget(target).scripts.find((script) => script.url && script.url.includes("breakpoint-detach-frame-script.js"));
    InspectorTest.expectNotNull(script, "Should find the frame's script.");
    if (!script)
        return;

    await target.DebuggerAgent.setBreakpointsActive.invoke({active: true});
    await target.DebuggerAgent.setBreakpoint.invoke({
        // The report() line in breakpoint-detach-frame-script.js; see the note at the top of it.
        location: {scriptId: script.id, lineNumber: 8, columnNumber: 0},
        options,
    });
};

});
