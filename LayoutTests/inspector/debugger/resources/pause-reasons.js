function triggerBreakpoint()
{
    1+1; // BREAKPOINT
}

function triggerException()
{
    [].x.x;
}

function triggerDebuggerStatement()
{
    debugger;
}

function triggerAssert()
{
    console.assert(false, "Assertion message");
}
