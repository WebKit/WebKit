
description("This tests querying usage and quota using Quota API.");

if (navigator.webkitTemporaryStorage) {
    window.jsTestIsAsync = true;
    // navigator.webkitTemporaryStorage.queryUsageAndQuota(usageCallback, errorCallback);
}

var worker = createWorker();

worker.postMessage("ping");
worker.postMessage("eval importScripts('worker-storagequota-query-usage.js');");
worker.postMessage("eval requestUsage(self.port || self)");
worker.onmessage = function(evt) {
    var match = /^result:(.*)/.exec(evt.data);
    if (match) {
        usageData = JSON.parse(match[1]);

        // Usage should be 0 (if other storage tests have correctly cleaned up their test data before exiting).
        shouldBe("usageData.usage", "0");

        // Quota value would vary depending on the test environment.
        shouldBeGreaterThanOrEqual("usageData.quota", "usageData.usage");
        worker.postMessage("close");
        finishJSTest();
    }
};

window.successfullyParsed = true;
