if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

self.jsTestIsAsync = true;

if (self.window)
    description("Basic Interface test for High Resolution Time APIs.");

debug("Performance");
shouldBeDefined(`Performance`);
shouldBeDefined(`Performance.prototype.now`);
shouldThrow(`new Performance()`);

debug("performance");
shouldBeDefined(`performance`);
shouldBeTrue(`performance instanceof Performance`);
shouldBeTrue(`typeof performance.now() === "number"`);
shouldBeTrue(`performance.now() <= performance.now()`);

self.time1 = performance.now();
setTimeout(function() {
    self.delta = performance.now() - time1;
    // Allow for ~20ms error.
    shouldBeTrue(`delta >= 95 && delta <= 120`);

    if (self.importScripts)
        finishJSTest();
    else
        testWorker();
}, 100);
