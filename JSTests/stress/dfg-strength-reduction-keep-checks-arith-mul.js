//@ runDefault("--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--useConcurrentJIT=0")
let v1 = 2.0;
for (let v2 = 0; v2 < 100; v2++) {
    let v3 = -1061384422;
    function f4(a5, a6, a7) {
        if (!(a5 == -4294967297)) {
        }
        a5 * a6;
        Math.abs(Math);
        v3++;
        return v1;
    }
    for (let v13 = 0; v13 < 100; v13++) {
        f4(v13, v1);
    }
}
v1++;
