function triggerDebuggerStatement() {
    let before = 1;
    debugger;
    let after = 2;
}

function triggerBreakpoint() {
    let alpha = 1;
    let beta = 2; // BREAKPOINT
    let gamma = 3;
}

function triggerException() {
    [].x.x;
}

function triggerAssertion() {
    console.assert(false, "Assertion");
}

onmessage = function(event) {
    if (event.data === "triggerDebuggerStatement")
        triggerDebuggerStatement();
    else if (event.data === "triggerBreakpoint")
        triggerBreakpoint();
    else if (event.data === "triggerException")
        triggerException();
    else if (event.data === "triggerAssertion")
        triggerAssertion();
    else if (event.data === "triggerResponse")
        postMessage("response");
}
