if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

if (self.window)
    description("Basic Interface test for user-timing APIs.");

debug("PerformanceMark");
shouldBeDefined("PerformanceMark");
shouldThrow(`new PerformanceMark()`);

debug("");
debug("PerformanceMeasure");
shouldBeDefined("PerformanceMeasure");
shouldThrow(`new PerformanceMeasure()`);

debug("");
debug("Performance extensions");
shouldBeDefined(`Performance.prototype.mark`);
shouldBeDefined(`Performance.prototype.measure`);
shouldBeDefined(`Performance.prototype.clearMarks`);
shouldBeDefined(`Performance.prototype.clearMeasures`);
shouldThrow(`performance.mark()`);
shouldNotThrow(`performance.mark("mark_name")`);
shouldThrow(`performance.measure()`);
shouldNotThrow(`performance.measure("measure_name")`);
shouldNotThrow(`performance.clearMarks()`);
shouldNotThrow(`performance.clearMarks("mark_name")`);
shouldNotThrow(`performance.clearMeasures()`);
shouldNotThrow(`performance.clearMeasures("measure_name")`);

if (self.importScripts)
    finishJSTest();
