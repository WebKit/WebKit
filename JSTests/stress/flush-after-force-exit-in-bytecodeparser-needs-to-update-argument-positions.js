//@ runDefault("--useConcurrentGC=0", "--thresholdForJITAfterWarmUp=10", "--thresholdForJITSoon=10", "--thresholdForOptimizeAfterWarmUp=20", "--thresholdForOptimizeAfterLongWarmUp=20", "--thresholdForOptimizeSoon=20", "--thresholdForFTLOptimizeAfterWarmUp=20", "--thresholdForFTLOptimizeSoon=20", "--maximumEvalCacheableSourceLength=150000", "--maxPerThreadStackUsage=1048576")

function runNearStackLimit(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            return f();
        }
    }
    return t();
};

runNearStackLimit(() => { });
runNearStackLimit(() => { });

function f2(a, b) {
    'use strict';
    try {
        a.push(arguments[0] + arguments[2] + a + undefinedVariable);
    } catch (e) { }
}

try {
    runNearStackLimit(() => {
        return f2(1, 2, 3);
    });
} catch (e) {}

try {
    runNearStackLimit();
} catch { }
