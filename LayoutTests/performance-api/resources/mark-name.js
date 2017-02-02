if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

self.jsTestIsAsync = true;

if (self.window)
    description("Tests performance.mark name restrictions apply only in Window.");

const reservedLegacyPerformanceTimingAttributeNames = [
    "navigationStart",
    "unloadEventStart",
    "unloadEventEnd",
    "redirectStart",
    "redirectEnd",
    "fetchStart",
    "domainLookupStart",
    "domainLookupEnd",
    "connectStart",
    "connectEnd",
    "secureConnectionStart",
    "requestStart",
    "responseStart",
    "responseEnd",
    "domLoading",
    "domInteractive",
    "domContentLoadedEventStart",
    "domContentLoadedEventEnd",
    "domComplete",
    "loadEventStart",
    "loadEventEnd",
];

let t = self.window ? shouldThrow : shouldNotThrow;
for (let name of reservedLegacyPerformanceTimingAttributeNames)
    t(`performance.mark("${name}")`);

shouldNotThrow(`performance.mark("mark_name")`);

if (self.importScripts)
    finishJSTest();
