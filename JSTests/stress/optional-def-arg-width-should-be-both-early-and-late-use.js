//@ runDefault("--useConcurrentJIT=false", "--thresholdForJITAfterWarmUp=10", "--thresholdForFTLOptimizeAfterWarmUp=1000")

function v0(v1) {
    function v7(v8) {
        function v12(v13, v14) {
            const v16 = v14 - -0x80000000;
            const v19 = [13.37, 13.37, 13.37];
            function v20() {
                return v16;
            }
            return v19;
        }
        return v8(v12, v1);
    }
    const v27 = v7(v7);
}
for (let i = 0; i < 100; i++) {
    v0(i);
}
