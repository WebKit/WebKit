if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

if (self.window)
    description("Basic Interface test for performance-timeline APIs.");

debug("PerformanceEntry");
shouldBeDefined("PerformanceEntry");
shouldBeTrue(`"name" in PerformanceEntry.prototype`);
shouldBeTrue(`"entryType" in PerformanceEntry.prototype`);
shouldBeTrue(`"startTime" in PerformanceEntry.prototype`);
shouldBeTrue(`"duration" in PerformanceEntry.prototype`);
shouldThrow(`new PerformanceEntry()`);

// NOTE: The APIs below may be going away. Replaced by PerformanceObserver.

debug("");
debug("Performance extensions");
shouldBeDefined(`Performance.prototype.getEntries`);
shouldBeDefined(`Performance.prototype.getEntriesByType`);
shouldBeDefined(`Performance.prototype.getEntriesByName`);

shouldBeTrue(`performance.getEntries() instanceof Array`);
shouldBeTrue(`performance.getEntries().length === 0`);
shouldNotThrow(`performance.mark("test");`);
shouldBeTrue(`performance.getEntries().length === 1`);
shouldBeTrue(`performance.getEntries()[0] instanceof PerformanceEntry`);
shouldBeEqualToString(`performance.getEntries()[0].name`, "test");
shouldBeEqualToString(`performance.getEntries()[0].entryType`, "mark");
shouldBeTrue(`typeof performance.getEntries()[0].startTime === "number"`);
shouldBeTrue(`typeof performance.getEntries()[0].duration === "number"`);

shouldThrow(`performance.getEntriesByType()`);
shouldBeTrue(`performance.getEntriesByType("not-real").length === 0`);
shouldBeTrue(`performance.getEntriesByType("mark").length === 1`);
shouldBeTrue(`performance.getEntriesByType("mark")[0] instanceof PerformanceEntry`);
shouldBeEqualToString(`performance.getEntriesByType("mark")[0].name`, "test");
shouldBeEqualToString(`performance.getEntriesByType("mark")[0].entryType`, "mark");
shouldBeTrue(`typeof performance.getEntriesByType("mark")[0].startTime === "number"`);
shouldBeTrue(`typeof performance.getEntriesByType("mark")[0].duration === "number"`);

shouldThrow(`performance.getEntriesByName()`);
shouldBeTrue(`performance.getEntriesByName("not-real").length === 0`);
shouldBeTrue(`performance.getEntriesByName("test").length === 1`);
shouldBeTrue(`performance.getEntriesByName("test")[0] instanceof PerformanceEntry`);
shouldBeEqualToString(`performance.getEntriesByName("test")[0].name`, "test");
shouldBeEqualToString(`performance.getEntriesByName("test")[0].entryType`, "mark");
shouldBeTrue(`typeof performance.getEntriesByName("test")[0].startTime === "number"`);
shouldBeTrue(`typeof performance.getEntriesByName("test")[0].duration === "number"`);
shouldBeTrue(`performance.getEntriesByName("test", "not-real").length === 0`);
shouldBeTrue(`performance.getEntriesByName("test", "mark").length === 1`);

if (self.importScripts)
    finishJSTest();
