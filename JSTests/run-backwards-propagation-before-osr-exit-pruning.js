//@ runDefault("--validateOptions=true", "--useConcurrentJIT=false", "--useConcurrentGC=false", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useFTLJIT=true")

function assert(b) {
    if (!b)
        throw new Error;
}
function main() {
    let v38;
    let v40;

    async function v24() {
        const v33 = false;
        const v34 = -v33;
        const v37 = typeof search;
        const v39 = v38 ? v30 : 1;
        v40 = v34;
            
        for (let v41 = 0; v41 != 100000; v41++) { }
    }
    [1,1,1].filter(v24);
    assert(Object.is(v40, -0) === true);
    assert(Object.is(v40, 0) === false);
}
main();
