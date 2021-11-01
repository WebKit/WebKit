//@ runDefault("--useConcurrentJIT=false", "--useConcurrentGC=false", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useConcurrentJIT=0", "--dumpFTLDisassembly=0", "--useFTLJIT=0")

function main() {
    async function v23(v24) {
        for (let v30 = 0; v30 < 60000; v30++) { } 
        ArrayBuffer.prototype.constructor = ArrayBuffer;
    }
    
    const v22 = [0, 0, 0]; 
    const v35 = v22.filter(v23);
    
    const v37 = [0, 0, 0]
    const v42 = new Uint8ClampedArray(v37);
    const v43 = new Uint32Array(v42);
}
main();
