//@ runDefault("--useConcurrentJIT=0", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForFTLOptimizeAfterWarmUp=1000")

function foo(a, b) {
    var r = 0;
    var w = a | 0;
    if (w === 0) {
        var x = w - 3;
        if (b)
            r = Math.abs(x);
    }
    return r;
}
noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo(0, 1);
    if (result !== 3)
        throw new Error("bad result at i=" + i + ": " + result);
}
