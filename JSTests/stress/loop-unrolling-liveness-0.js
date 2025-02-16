//@ runDefault("--useConcurrentJIT=0", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForFTLOptimizeAfterWarmUp=1000")
function f1(a2, a3, a4) {
    let v5 = a2 && 99;
    let v6 = 0;
    let v8 = v5++;
    do {
        v8 &&= v5;
        v6++;
    } while (v6 < 4);
    return v6;
}
for (let v10 = 0; v10 < 100; v10++) {
    f1();
}
