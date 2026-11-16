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

function hasInitializedSwipeTest()
{
    return ("swipeLogging" in window.localStorage);
}

function startMeasuringDuration(key)
{
    window.localStorage[key + "swipeStartTime"] = Date.now();
}

function measuredDurationShouldBeLessThan(key, timeInMS, message)
{
    let duration = Date.now() - window.localStorage[key + "swipeStartTime"];
    if (duration >= timeInMS)
        log("Failure. " + message + " (expected: " + timeInMS + ", actual: " + duration + ")");
}

function startSwipeGesture(callback)
{
    log("startSwipeGesture");
    testRunner.runUIScript(`
    (function() {
        uiController.beginBackSwipe(function() {
            uiController.uiScriptComplete();
        });
    })();`, callback || function () {});
}

function completeSwipeGesture(callback)
{
    log("completeSwipeGesture");
    testRunner.runUIScript(`
    (function() {
        uiController.completeBackSwipe(function() {
            uiController.uiScriptComplete();
        });
    })();`, callback || function () {});
}

function playEventStream(stream, callback)
{
    log("playEventStream");
    if (testRunner.isIOSFamily) {
        // FIXME: This test should probably not log playEventStream
        // on iOS, where it doesn't actually do it.
        setTimeout(callback, 0);
        return;
    }
    testRunner.runUIScript(`
    (function() {
        uiController.playBackEventStream(\`${stream}\`, function() {
            uiController.uiScriptComplete();
        });
    })();`, callback || function () {});
}
