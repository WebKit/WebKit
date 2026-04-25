var frameGlobalValue = "cross-origin-frame-value";

function triggerDebugger() {
    debugger;
    return "after-debugger";
}

function breakpointTarget(x) {
    let localVar = x * 2;
    return localVar + 1;
}
