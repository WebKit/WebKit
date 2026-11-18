// Each frame runs only the functions named after it, so a sample can be traced back to the frame that ran it.

// Long enough that the sampling profiler, which samples about once per millisecond, usually takes some samples during
// it. It can still miss a whole loop, so the tests check samples through FrameScriptProfilerTest.repeatWhileUnsampled.
const sampledWorkMilliseconds = 100;

function busyWait(milliseconds)
{
    let end = performance.now() + milliseconds;
    while (performance.now() < end) { }
}

function mainFrameWork()
{
    busyWait(sampledWorkMilliseconds);
}

function mainFrameWorkAfterRemoval()
{
    busyWait(sampledWorkMilliseconds);
}

function iframeWork()
{
    busyWait(sampledWorkMilliseconds);
}

function iframeWorkBeforeNavigation()
{
    busyWait(sampledWorkMilliseconds);
}

function iframeWorkAfterNavigation()
{
    busyWait(sampledWorkMilliseconds);
}

// Long enough to tell its Microtask event apart from the ones of short promise reactions.
function queueIFrameMicrotask()
{
    queueMicrotask(() => busyWait(5));
}

// Resolves after the function has run in a timer, so that it is recorded as a TimerFired record.
function runInTimer(functionName)
{
    return new Promise((resolve) => {
        setTimeout(() => {
            window[functionName]();
            resolve();
        });
    });
}
