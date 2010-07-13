description("This test checks that all of the <a href='http://dev.w3.org/2006/webapi/WebTiming/'>Web Timing</a> attributes are available and have reasonable values in the right order.");

var performance = window.webkitPerformance || {};
var timing = performance.timing || {};

// Get the order of magnitude correct without a chance for flakiness.
var oneHourMilliseconds = 60 * 60 * 1000;
var currentUTC = (new Date()).getTime();
var oneHourAgoUTC = currentUTC - oneHourMilliseconds;

function sleepFiftyMilliseconds() {
  var endTime = (new Date()).getTime() + 50;
  while ((new Date().getTime() < endTime)) { }
}
window.addEventListener("load", sleepFiftyMilliseconds, false);

// FIXME: Move this to js-test-pre.js if it is needed by other tests.
function shouldBeGreaterThanOrEqual(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeGreaterThanOrEqual expects string arguments");

    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);

    if (exception)
        testFailed(_a + " should be >= " + _b + ". Threw exception " + exception);
    else if (typeof _av == "undefined" || _av < _bv)
        testFailed(_a + " should be >= " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is >= " + _b);
}

function checkTimingBeforeLoad()
{
    shouldBeGreaterThanOrEqual("timing.navigationStart", "oneHourAgoUTC");

    shouldBeGreaterThanOrEqual("timing.unloadEventEnd", "timing.navigationStart");

    shouldBeGreaterThanOrEqual("timing.fetchStart", "timing.navigationStart");

    shouldBe("timing.domainLookupStart", "0");
    shouldBe("timing.domainLookupEnd", "0");

    shouldBe("timing.connectStart", "0");
    shouldBe("timing.connectEnd", "0");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.navigationStart");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBeGreaterThanOrEqual("timing.responseStart", "timing.requestEnd");

    shouldBe("timing.loadEventStart", "0");
    shouldBe("timing.loadEventEnd", "0");

    window.addEventListener("load", checkWebTimingOnLoad, false);
}

function checkWebTimingOnLoad()
{
    shouldBeGreaterThanOrEqual("timing.navigationStart", "oneHourAgoUTC");

    shouldBeGreaterThanOrEqual("timing.unloadEventEnd", "timing.navigationStart");

    shouldBeGreaterThanOrEqual("timing.fetchStart", "timing.navigationStart");

    shouldBe("timing.domainLookupStart", "0");
    shouldBe("timing.domainLookupEnd", "0");

    shouldBe("timing.connectStart", "0");
    shouldBe("timing.connectEnd", "0");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.navigationStart");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBeGreaterThanOrEqual("timing.responseStart", "timing.requestEnd");
    shouldBeGreaterThanOrEqual("timing.responseEnd", "timing.responseStart");

    shouldBeGreaterThanOrEqual("timing.loadEventStart", "timing.responseEnd");
    shouldBe("timing.loadEventEnd", "0");

    setTimeout("checkWebTimingAfterLoad()", 0);
}

function checkWebTimingAfterLoad()
{
    shouldBeGreaterThanOrEqual("timing.navigationStart", "oneHourAgoUTC");

    shouldBeGreaterThanOrEqual("timing.unloadEventEnd", "timing.navigationStart");

    shouldBeGreaterThanOrEqual("timing.fetchStart", "timing.navigationStart");

    shouldBe("timing.domainLookupStart", "0");
    shouldBe("timing.domainLookupEnd", "0");

    shouldBe("timing.connectStart", "0");
    shouldBe("timing.connectEnd", "0");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.navigationStart");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBeGreaterThanOrEqual("timing.responseStart", "timing.requestEnd");
    shouldBeGreaterThanOrEqual("timing.responseEnd", "timing.responseStart");

    shouldBeGreaterThanOrEqual("timing.loadEventStart", "timing.responseEnd");
    shouldBeGreaterThanOrEqual("timing.loadEventEnd", "timing.loadEventStart + 50");

    finishJSTest();
}

jsTestIsAsync = true;
checkTimingBeforeLoad();

var successfullyParsed = true;
