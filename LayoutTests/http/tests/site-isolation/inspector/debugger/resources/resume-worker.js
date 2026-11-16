globalThis.workerTicks = 0;
setInterval(() => { ++globalThis.workerTicks; }, 50);

function triggerDebugger()
{
    debugger;
    return "after-debugger";
}
