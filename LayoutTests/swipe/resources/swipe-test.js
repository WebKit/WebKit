function shouldBe(expected, actual, message)
{
    if (expected != actual)
        log("Failure. " + message + " (expected: " + expected + ", actual: " + actual + ")");
}

function log(s)
{
    window.localStorage["swipeLogging"] += s + "<br/>";
}

function dumpLog()
{
    window.document.body.innerHTML = window.localStorage["swipeLogging"];
}

function testComplete()
{
    dumpLog();
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
