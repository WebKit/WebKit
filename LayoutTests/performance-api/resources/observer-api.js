if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

if (self.window)
    description("Basic Interface test for PerformanceObserver APIs.");

debug("PerformanceObserver");
shouldBeDefined(`PerformanceObserver`);
shouldBeDefined(`PerformanceObserver.prototype.observe`);
shouldBeDefined(`PerformanceObserver.prototype.disconnect`);
shouldThrow(`PerformanceObserver()`);
shouldThrow(`new PerformanceObserver()`);
shouldThrow(`new PerformanceObserver(1)`);
shouldNotThrow(`observer = new PerformanceObserver(function() {})`);
shouldThrow(`observer.observe()`);
shouldThrow(`observer.observe("mark")`);
shouldThrow(`observer.observe({})`);
shouldThrow(`observer.observe({entryTypes:"mark"})`);
shouldNotThrow(`observer.observe({entryTypes:[]})`);
shouldNotThrow(`observer.observe({entryTypes:["not-real"]})`);
shouldNotThrow(`observer.observe({entryTypes:["mark"]})`);
shouldNotThrow(`observer.observe({entryTypes:["mark", "not-real"]})`);
shouldNotThrow(`observer.observe({entryTypes:["mark", "measure"]})`);
shouldNotThrow(`observer.disconnect()`);
shouldNotThrow(`observer.disconnect()`);

debug("");
debug("PerformanceObserverEntryList");
shouldBeDefined(`PerformanceObserverEntryList`);
shouldBeDefined(`PerformanceObserverEntryList.prototype.getEntries`);
shouldBeDefined(`PerformanceObserverEntryList.prototype.getEntriesByType`);
shouldBeDefined(`PerformanceObserverEntryList.prototype.getEntriesByName`);
shouldThrow(`new PerformanceObserverEntryList()`);

if (self.importScripts)
    finishJSTest();
