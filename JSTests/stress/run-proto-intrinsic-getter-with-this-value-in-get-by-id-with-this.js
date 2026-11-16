//@ runDefault("--forcePolyProto=true", "--validateOptions=true", "--useConcurrentJIT=false", "--useConcurrentGC=false", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useFTLJIT=true")

function assert(b) {
    if (!b)
        throw new Error;
}

function main() {
    let v41;

    v37 = class V37 {
        constructor() {
            v41 = super.__proto__;
        }
    };

    for (let v70 = 0; v70 < 100; v70++) {
        new v37();
        assert(v41 !== null);
    }

}
noDFG(main);
noFTL(main);
main();
