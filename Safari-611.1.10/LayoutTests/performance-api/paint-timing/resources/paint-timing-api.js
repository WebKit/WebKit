if (self.importScripts)
    importScripts("../../../resources/js-test-pre.js");

if (self.window)
    description("Basic Interface test for paint-timing APIs.");

debug("PerformancePaintTiming");
shouldBeDefined("PerformancePaintTiming");
shouldBeDefined(`PerformancePaintTiming.prototype.toJSON`);
shouldThrow(`new PerformancePaintTiming()`);

if (self.importScripts)
    finishJSTest();
