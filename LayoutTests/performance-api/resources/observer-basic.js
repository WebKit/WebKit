if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

self.jsTestIsAsync = true;

if (self.window)
    description("Basic Behavior test for PerformanceObserver APIs.");

// PerformanceObservers that are not actively observing should not fire.
let observerNotObserving = new PerformanceObserver(function() {
    testFailed("PerformanceObserver never registered should not be called");
});

let observerNotObserving2 = new PerformanceObserver(function() {
    testFailed("PerformanceObserver not actively observing should not be called");
});
observerNotObserving2.observe({entryTypes: ["mark"]});
observerNotObserving2.disconnect();

// PerformanceObservers for different entry types should not fire.
let observerNotMarks = new PerformanceObserver(function() {
    testFailed("PerformanceObserver observing measures should not be called");
});

// Observer sees marks while it is registered.
performance.mark("mark1");
self.observer = new PerformanceObserver(function(list, obs) {
    self.argumentsLength = arguments.length;
    self.list = list;
    self.obs = obs;
    self.thisObject = this;
    
    debug("Inside PerformanceObserver callback");
    shouldBeTrue(`argumentsLength === 2`);
    shouldBeTrue(`list instanceof PerformanceObserverEntryList`);
    shouldBeTrue(`obs instanceof PerformanceObserver`);
    shouldBeTrue(`obs === observer`);
    // FIXME: <https://webkit.org/b/167549> Invoking generated callback should allow setting the `this` object
    shouldBeTrue(`thisObject instanceof PerformanceObserver`);
    shouldBeTrue(`thisObject === observer`);

    debug("");
    debug("PerformanceObserverEntryList APIs");

    shouldBeTrue(`list.getEntries() instanceof Array`);
    shouldBeTrue(`list.getEntries().length === 2`);
    shouldBeTrue(`list.getEntries()[0] instanceof PerformanceEntry`);
    shouldBeEqualToString(`list.getEntries()[0].name`, "mark3");
    shouldBeEqualToString(`list.getEntries()[1].name`, "mark4");
    shouldBeTrue(`list.getEntries()[0].startTime <= list.getEntries()[1].startTime`);

    shouldThrow(`list.getEntriesByType()`);
    shouldBeTrue(`list.getEntriesByType("not-real").length === 0`);
    shouldBeTrue(`list.getEntriesByType("mark").length === 2`);
    shouldBeTrue(`list.getEntriesByType("mark")[0] instanceof PerformanceEntry`);
    shouldBeEqualToString(`list.getEntriesByType("mark")[0].name`, "mark3");
    shouldBeEqualToString(`list.getEntriesByType("mark")[1].name`, "mark4");

    shouldThrow(`list.getEntriesByName()`);
    shouldBeTrue(`list.getEntriesByName("not-real").length === 0`);
    shouldBeTrue(`list.getEntriesByName("mark1").length === 0`);
    shouldBeTrue(`list.getEntriesByName("mark3").length === 1`);
    shouldBeTrue(`list.getEntriesByName("mark3")[0] instanceof PerformanceEntry`);
    shouldBeEqualToString(`list.getEntriesByName("mark3")[0].name`, "mark3");
    shouldBeTrue(`list.getEntriesByName("mark4").length === 1`);
    shouldBeTrue(`list.getEntriesByName("mark4")[0] instanceof PerformanceEntry`);
    shouldBeEqualToString(`list.getEntriesByName("mark4")[0].name`, "mark4");
    
    shouldThrow(`list.getEntriesByName()`);
    shouldBeTrue(`list.getEntriesByName("not-real").length === 0`);
    shouldBeTrue(`list.getEntriesByName("mark1").length === 0`);
    shouldBeTrue(`list.getEntriesByName("mark3").length === 1`);
    shouldBeTrue(`list.getEntriesByName("mark3")[0] instanceof PerformanceEntry`);
    shouldBeEqualToString(`list.getEntriesByName("mark3")[0].name`, "mark3");
    shouldBeTrue(`list.getEntriesByName("mark4").length === 1`);
    shouldBeTrue(`list.getEntriesByName("mark4")[0] instanceof PerformanceEntry`);
    shouldBeEqualToString(`list.getEntriesByName("mark4")[0].name`, "mark4");

    shouldBeTrue(`list.getEntriesByName("mark3", "not-real").length === 0`);
    shouldBeTrue(`list.getEntriesByName("mark3", "mark").length === 1`);
    shouldBeTrue(`list.getEntriesByName(null, "mark").length === 0`);
    shouldBeTrue(`list.getEntriesByName(undefined, "mark").length === 0`);

    if (self.importScripts)
        finishJSTest();
    else
        testWorker();
});
performance.mark("mark2");
observer.observe({entryTypes: ["mark"]});
performance.mark("mark3");
performance.mark("mark4");
