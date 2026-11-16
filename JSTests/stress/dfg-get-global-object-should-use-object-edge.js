//@ runDefault("--validateOptions=true", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useConcurrentJIT=0")

function opt(arg) {
    try {
        arg.test({ test: "padEnd" });
    } catch (e) { }
    arg.test(/123/);
}

for (let i = 0; i < 50; i++) {
    opt({ test: opt });
}
