function getDeltas(direction) {
    var deltaX = 0;
    var deltaY = 0;
    if (direction == "down")
        deltaY = -5
    else if (direction == "up")
        deltaY = 5;
    else if (direction == "right")
        deltaX = -5;
    else if (direction == "left")
        deltaX = 5;
    return {
        X: deltaX,
        Y: deltaY
    };
}

async function mouseWheelScrollAndWait(x, y, beginX, beginY, deltaX, deltaY)
{
    if (beginX === undefined)
        beginX = 0;
    if (beginY === undefined)
        beginY = -1;
    if (deltaX === undefined)
        deltaX = 0;
    if (deltaY === undefined)
        deltaY = -10;

    await UIHelper.startMonitoringWheelEvents();
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseScrollByWithWheelAndMomentumPhases(beginX, beginY, "began", "none");
    eventSender.mouseScrollByWithWheelAndMomentumPhases(deltaX, deltaY, "changed", "none");
    eventSender.mouseScrollByWithWheelAndMomentumPhases(0, 0, "ended", "none");

    // FIXME: Use UIHelper.waitForScrollCompletion() (but doing so causes timeouts).
    return new Promise(resolve => {
        setTimeout(() => {
            requestAnimationFrame(resolve);
        }, 500);
    });
}
