function setEnableFeature(enable, completionHandler) {
    if (typeof completionHandler !== "function")
        testFailed("setEnableFeature() requires a completion handler function.");
    if (enable) {
        internals.setTrackingPreventionEnabled(true);
        testRunner.setStatisticsIsRunningTest(true);
        completionHandler();
    } else {
        testRunner.statisticsResetToConsistentState(function() {
            testRunner.setStatisticsIsRunningTest(false);
            internals.setTrackingPreventionEnabled(false);
            completionHandler();
        });
    }
}

async function resetCookiesITP() {
    var testURL = "http://127.0.0.1:8000";
    console.assert(testURL == document.location.origin);

    function setUp() {
        return new Promise((resolve) => {
            if (window.testRunner) {
                testRunner.setAlwaysAcceptCookies(true);
            }
            resolve();
        });
    }

    function cleanUp() {
        return new Promise((resolve) => {
            if (window.testRunner)
                testRunner.setAlwaysAcceptCookies(false);
            resolve();
        });
    }

    let promise = setUp();
    promise = promise.then(() => {
        return new Promise((resolve, reject) => {
            window.addEventListener("message", (messageEvent) => resolve(messageEvent), {capture: true, once: true});
            const element = document.createElement("iframe");
            element.src = "http://127.0.0.1:8000/cookies/resources/delete-cookie.py";
            document.body.appendChild(element);
        });
    });
    return promise.then(cleanUp);
}

