importScripts("/resources/testharness.js");
importScripts("resources/rt-utilities.js?pipe=sub");

(function() {
    if (!testNecessaryPerformanceFeatures()) {
        done();
        return;
    }

    importScripts("rt-cors.js");

    done();
})();
