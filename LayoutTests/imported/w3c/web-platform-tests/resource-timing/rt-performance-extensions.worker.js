importScripts("/resources/testharness.js");
importScripts("resources/rt-utilities.js");

(function() {
    if (!testNecessaryPerformanceFeatures()) {
        done();
        return;
    }

    importScripts("rt-performance-extensions.js");

    done();
})();
