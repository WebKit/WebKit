importScripts("/resources/testharness.js");
importScripts("resources/rt-utilities.sub.js");

(function() {
    if (!testNecessaryPerformanceFeatures()) {
        done();
        return;
    }

    importScripts("rt-cors.js");

    done();
})();
