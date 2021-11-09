//@ runDefault("--validateOptions=true --useConcurrentJIT=false", "--useConcurrentGC=false", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useFTLJIT=true")

function main() {
    let v162;
    const v25 = {__proto__:"name"};
    
    for (let v113 = 0; v113 < 255; v113++) {
        const v141 = new Proxy(Object,v25);
        const v145 = v141["bind"]();
        // when running with FTL, the previous line raises a JS exception:
        // TypeError: |this| is not a function inside Function.prototype.bind
        // without FTL or in v8 this doesn't throw.

    }   
}
main();
