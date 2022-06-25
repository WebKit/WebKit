//@ runDefault("--validateOptions=true", "--useConcurrentJIT=false", "--useConcurrentGC=false", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useFTLJIT=1")

function assert(b) {
    if (!b)
        throw new Error;
}

function main() {
    let result;
    for (let i = 0; i < 7; ++i) {
        function f() {
            "a".charCodeAt(undefined);
            const v44 = new Date(123);
            result = -v44;
            for (let j = -4096; j < 100; j++) { } 
        }   
        noInline(f);
        f();
        assert(result === -123);
    }
}
main();
