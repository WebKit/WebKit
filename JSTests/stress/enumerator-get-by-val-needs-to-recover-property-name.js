//@ runDefault("--validateOptions=true", "--useConcurrentJIT=false", "--useConcurrentGC=false", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useFTLJIT=true")

function assert(b) {
    if (!b)
        throw new Error;
}

function main() {
    let result;
    const v35 = [0, 0, {b:"AAAAA"}];
    
    async function v36(arr) {
        edenGC();  // This is needed
        for (let i = 0; i < 2; i++) {
            const v201 = ` 
                var someVar; // this is needed

                for (let j = 0; j < 60000; j++) { }

                const v222 = {"__proto__":[[]], "a":0, "b":0};
                for (const prop in v222) {
                    result = arr[prop];
                    v222.__proto__ = {};
                }
            `;
            eval(v201); // moving code out of eval breaks differential
        }
    }
    v35.filter(v36);
    assert(result === "AAAAA");
}
main();
