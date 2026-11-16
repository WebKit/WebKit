// frame-target-timestamps.html sets probes on these lines by line number; keep them in sync.
function sampleTime()
{
    let now = performance.now();
    return now;
}

function pauseHere()
{
    debugger;
}
