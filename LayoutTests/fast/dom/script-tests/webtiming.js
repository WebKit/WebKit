description("This test checks that all of the <a href='http://dev.w3.org/2006/webapi/WebTiming/'>Web Timing</a> attributes are available and have reasonable values in the right order.");

var performance = window.webkitPerformance || {};
var navigation = performance.navigation || {};
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

function checkTimingBeforeLoad()
{
    shouldBeGreaterThanOrEqual("timing.navigationStart", "oneHourAgoUTC");

    shouldBeGreaterThanOrEqual("timing.unloadEventEnd", "timing.navigationStart");

    shouldBe("timing.redirectStart", "0");
    shouldBe("timing.redirectEnd", "0");
    shouldBe("navigation.redirectCount", "0");

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

    shouldBe("timing.redirectStart", "0");
    shouldBe("timing.redirectEnd", "0");
    shouldBe("navigation.redirectCount", "0");

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

    shouldBe("timing.redirectStart", "0");
    shouldBe("timing.redirectEnd", "0");
    shouldBe("navigation.redirectCount", "0");

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
