//@ runDefault("--validateOptions=true", "--useConcurrentJIT=false", "--useConcurrentGC=false", "--validateBCE=true", "--thresholdForJITSoon=1", "--thresholdForJITAfterWarmUp=7", "--thresholdForOptimizeAfterWarmUp=7", "--thresholdForOptimizeAfterLongWarmUp=7", "--thresholdForOptimizeSoon=1", "--thresholdForFTLOptimizeAfterWarmUp=10")

function assert(b) {
    if (!b)
        throw new Error;
}

function main() {
    let v17 = {__proto__:[42,1]};
    v17[2] = 4;
        
    let v92 = 0;
    for (let v95 = 0; v95 < 100; v95++) {
        function doEvery(e, i) {
            assert(e === 42);
            assert(i === 0);
            function doMap() {
                v139 = v92++;
            }   
            noInline(doMap);
            [0].map(doMap);
        }   
        noInline(doEvery);
        v17.every(doEvery);
    }   
    assert(v139 === 99);
}
main();
