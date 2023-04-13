function shouldBe(expected, actual, message)
{
    if (expected != actual)
        log("Failure. " + message + " (expected: " + expected + ", actual: " + actual + ")");
}

function log(s)
{
    window.localStorage["swipeLogging"] += s + "<br/>";
}

function dumpLog(logContainer)
{
    if (!logContainer)
        logContainer = document.body;
    logContainer.innerHTML = window.localStorage["swipeLogging"];
}

function testComplete(logContainer)
{
    dumpLog(logContainer);
    window.testRunner.notifyDone();
}

function initializeSwipeTest()
{
    window.localStorage["swipeLogging"] = "";
    testRunner.setNavigationGesturesEnabled(true);
    testRunner.clearBackForwardList();
}

function startMeasuringDuration(key)
{
    window.localStorage[key + "swipeStartTime"] = Date.now();
}

function measuredDurationShouldBeLessThan(key, timeInMS, message)
{
    var duration = Date.now() - window.localStorage[key + "swipeStartTime"];
    if (duration >= timeInMS)
        log("Failure. " + message + " (expected: " + timeInMS + ", actual: " + duration + ")");
}

async function startSlowSwipeGesture()
{
    if (!window.eventSender)
        return;

    log("startSlowSwipeGesture");

    await UIHelper.ensurePresentationUpdate();

    // Similar to uiController.beginBackSwipe(), but with a gap between events to allow
    // for DOM wheel event handlers to fire.
    eventSender.mouseMoveTo(400, 300);
    eventSender.mouseScrollByWithWheelAndMomentumPhases(1, 0, "began", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(8, 0, "changed", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(10, 0, "changed", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(10, 0, "changed", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(10, 0, "changed", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(10, 0, "changed", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(10, 0, "changed", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(5, 0, "changed", "none");
    await UIHelper.renderingUpdate();
    eventSender.mouseScrollByWithWheelAndMomentumPhases(0, 0, "ended", "none");
}

async function startSwipeGesture()
{
    log("startSwipeGesture");

    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.beginBackSwipe(function() {
                    uiController.uiScriptComplete();
                });
            })();
        `, resolve);
    });
}

async function completeSwipeGesture()
{
    log("completeSwipeGesture");

    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.completeBackSwipe(function() {
                    uiController.uiScriptComplete();
                });
            })();
        `, resolve);
    });
}

async function playEventStream(stream)
{
    log("playEventStream");
    if (testRunner.isIOSFamily) {
        // FIXME: This test should probably not log playEventStream
        // on iOS, where it doesn't actually do it.
        const timeout = 0;
        return new Promise(resolve => setTimeout(resolve, timeout));
    }

    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.playBackEventStream(\`${stream}\`, function() {
                    uiController.uiScriptComplete();
                });
            })();
        `, resolve);
    });
}
