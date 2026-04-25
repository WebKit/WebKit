if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

if (self.window)
    description("Basic Interface test for resource-timing APIs.");

debug("PerformanceResourceTiming");
shouldBeDefined("PerformanceResourceTiming");
shouldBeTrue(`"initiatorType" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"nextHopProtocol" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"workerStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"redirectStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"redirectEnd" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"fetchStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"domainLookupStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"domainLookupEnd" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"connectStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"connectEnd" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"secureConnectionStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"requestStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"responseStart" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"responseEnd" in PerformanceResourceTiming.prototype`);
shouldBeTrue(`"transferSize" in PerformanceResourceTiming.prototype`); // Unimplemented.
shouldBeTrue(`"encodedBodySize" in PerformanceResourceTiming.prototype`); // Unimplemented.
shouldBeTrue(`"decodedBodySize" in PerformanceResourceTiming.prototype`); // Unimplemented.
shouldBeDefined(`PerformanceResourceTiming.prototype.toJSON`);
shouldThrow(`new PerformanceResourceTiming()`);

debug("");
debug("Performance extensions");
shouldBeDefined(`Performance.prototype.clearResourceTimings`);
shouldBeDefined(`Performance.prototype.setResourceTimingBufferSize`);
shouldBeDefined(`performance.onresourcetimingbufferfull`);
shouldNotThrow(`performance.clearResourceTimings()`);
shouldThrow(`performance.setResourceTimingBufferSize()`);
shouldNotThrow(`performance.setResourceTimingBufferSize(100)`);

if (self.importScripts)
    finishJSTest();
