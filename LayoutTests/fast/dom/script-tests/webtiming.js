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

    shouldBeGreaterThanOrEqual("timing.domainLookupStart", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domainLookupEnd", "timing.domainLookupStart");

    shouldBeGreaterThanOrEqual("timing.connectStart", "timing.domainLookupEnd");
    shouldBeGreaterThanOrEqual("timing.connectEnd", "timing.connectStart");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.connectEnd");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBe("timing.responseStart", "timing.requestEnd");

    shouldBeGreaterThanOrEqual("timing.domLoading", "timing.fetchStart");
    shouldBe("timing.domInteractive", "0");
    shouldBe("timing.domContentLoadedStart", "0");
    shouldBe("timing.domContentLoadedEnd", "0");
    shouldBe("timing.domComplete", "0");

    shouldBe("timing.loadEventStart", "0");
    shouldBe("timing.loadEventEnd", "0");
}

function checkTimingWhileDeferred()
{
    shouldBeGreaterThanOrEqual("timing.navigationStart", "oneHourAgoUTC");

    shouldBeGreaterThanOrEqual("timing.unloadEventEnd", "timing.navigationStart");

    shouldBe("timing.redirectStart", "0");
    shouldBe("timing.redirectEnd", "0");
    shouldBe("navigation.redirectCount", "0");

    shouldBeGreaterThanOrEqual("timing.fetchStart", "timing.navigationStart");

    shouldBeGreaterThanOrEqual("timing.domainLookupStart", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domainLookupEnd", "timing.domainLookupStart");

    shouldBeGreaterThanOrEqual("timing.connectStart", "timing.domainLookupEnd");
    shouldBeGreaterThanOrEqual("timing.connectEnd", "timing.connectStart");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.connectEnd");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBe("timing.responseStart", "timing.requestEnd");

    shouldBeGreaterThanOrEqual("timing.domLoading", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domInteractive", "timing.domLoading");
    shouldBe("timing.domContentLoadedStart", "0");
    shouldBe("timing.domContentLoadedEnd", "0");
    shouldBe("timing.domComplete", "0");

    shouldBe("timing.loadEventStart", "0");
    shouldBe("timing.loadEventEnd", "0");

    window.addEventListener("DOMContentLoaded", checkWebTimingOnDOMContentLoaded, false);
}

function checkWebTimingOnDOMContentLoaded() {
    shouldBeGreaterThanOrEqual("timing.navigationStart", "oneHourAgoUTC");

    shouldBeGreaterThanOrEqual("timing.unloadEventEnd", "timing.navigationStart");

    shouldBe("timing.redirectStart", "0");
    shouldBe("timing.redirectEnd", "0");
    shouldBe("navigation.redirectCount", "0");

    shouldBeGreaterThanOrEqual("timing.fetchStart", "timing.navigationStart");

    shouldBeGreaterThanOrEqual("timing.domainLookupStart", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domainLookupEnd", "timing.domainLookupStart");

    shouldBeGreaterThanOrEqual("timing.connectStart", "timing.domainLookupEnd");
    shouldBeGreaterThanOrEqual("timing.connectEnd", "timing.connectStart");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.connectEnd");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBe("timing.responseStart", "timing.requestEnd");

    shouldBeGreaterThanOrEqual("timing.domLoading", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domInteractive", "timing.domLoading");
    shouldBeGreaterThanOrEqual("timing.domContentLoadedStart", "timing.domInteractive");
    shouldBe("timing.domContentLoadedEnd", "0");
    shouldBe("timing.domComplete", "0");

    shouldBe("timing.loadEventStart", "0");
    shouldBe("timing.loadEventEnd", "0");

    var body = document.getElementsByTagName("body")[0];
    var script = document.createElement("script");
    script.async = true;
    script.type = "text/javascript";
    script.src = "script-tests/webtiming-async.js";
    body.appendChild(script);
}

function checkWebTimingWhileAsync()
{
    shouldBeGreaterThanOrEqual("timing.navigationStart", "oneHourAgoUTC");

    shouldBeGreaterThanOrEqual("timing.unloadEventEnd", "timing.navigationStart");

    shouldBe("timing.redirectStart", "0");
    shouldBe("timing.redirectEnd", "0");
    shouldBe("navigation.redirectCount", "0");

    shouldBeGreaterThanOrEqual("timing.fetchStart", "timing.navigationStart");

    shouldBeGreaterThanOrEqual("timing.domainLookupStart", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domainLookupEnd", "timing.domainLookupStart");

    shouldBeGreaterThanOrEqual("timing.connectStart", "timing.domainLookupEnd");
    shouldBeGreaterThanOrEqual("timing.connectEnd", "timing.connectStart");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.connectEnd");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBe("timing.responseStart", "timing.requestEnd");

    shouldBeGreaterThanOrEqual("timing.domLoading", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domInteractive", "timing.responseEnd");
    shouldBeGreaterThanOrEqual("timing.domContentLoadedStart", "timing.domInteractive");
    shouldBeGreaterThanOrEqual("timing.domContentLoadedEnd", "timing.domContentLoadedStart");
    shouldBe("timing.domComplete", "0");

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

    shouldBeGreaterThanOrEqual("timing.domainLookupStart", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domainLookupEnd", "timing.domainLookupStart");

    shouldBeGreaterThanOrEqual("timing.connectStart", "timing.domainLookupEnd");
    shouldBeGreaterThanOrEqual("timing.connectEnd", "timing.connectStart");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.connectEnd");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBe("timing.responseStart", "timing.requestEnd");
    shouldBeGreaterThanOrEqual("timing.responseEnd", "timing.responseStart");

    shouldBeGreaterThanOrEqual("timing.domLoading", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domInteractive", "timing.responseEnd");
    shouldBeGreaterThanOrEqual("timing.domContentLoadedStart", "timing.domInteractive");
    shouldBeGreaterThanOrEqual("timing.domContentLoadedEnd", "timing.domContentLoadedStart");
    shouldBeGreaterThanOrEqual("timing.domComplete", "timing.domContentLoadedEnd");

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

    shouldBeGreaterThanOrEqual("timing.domainLookupStart", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domainLookupEnd", "timing.domainLookupStart");

    shouldBeGreaterThanOrEqual("timing.connectStart", "timing.domainLookupEnd");
    shouldBeGreaterThanOrEqual("timing.connectEnd", "timing.connectStart");

    shouldBeGreaterThanOrEqual("timing.requestStart", "timing.connectEnd");
    shouldBeGreaterThanOrEqual("timing.requestEnd", "timing.requestStart");

    shouldBe("timing.responseStart", "timing.requestEnd");
    shouldBeGreaterThanOrEqual("timing.responseEnd", "timing.responseStart");

    shouldBeGreaterThanOrEqual("timing.domLoading", "timing.fetchStart");
    shouldBeGreaterThanOrEqual("timing.domInteractive", "timing.responseEnd");
    shouldBeGreaterThanOrEqual("timing.domContentLoadedStart", "timing.domInteractive");
    shouldBeGreaterThanOrEqual("timing.domContentLoadedEnd", "timing.domContentLoadedStart");
    shouldBeGreaterThanOrEqual("timing.domComplete", "timing.domContentLoadedEnd");

    shouldBeGreaterThanOrEqual("timing.loadEventStart", "timing.responseEnd");
    shouldBeGreaterThanOrEqual("timing.loadEventEnd", "timing.loadEventStart + 50");

    finishJSTest();
}

jsTestIsAsync = true;
checkTimingBeforeLoad();

var successfullyParsed = true;
