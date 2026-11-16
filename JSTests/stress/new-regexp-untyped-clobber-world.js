//@ runDefault("--useConcurrentJIT=0", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100")
for (let v1 = 0; v1 < testLoopCount; v1++) {
    ("object").matchAll();
}
