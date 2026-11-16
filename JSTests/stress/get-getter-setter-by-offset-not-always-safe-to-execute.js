//@ runDefault("--thresholdForOptimizeSoon=10", "--useConcurrentJIT=0", "--useConcurrentGC=0", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForFTLOptimizeAfterWarmUp=1000")

function main() {
    function v1(v2) {
        try {
            const v3 = v2.__proto__;
            v2.e = 0x12341234;
            for (let v9 = 0; v9 < 10; v9++) {
                v1(v3);
            }
            for (let v20 = 0; v20 < 6; v20++) {
                const v21 = v2.__proto__;
                const v24 = {a:13.37};
                const v26 = [v24];
            }
        } catch(v29) {}
    }
    v1(v1);
}
noDFG(main);
noFTL(main);
main();
