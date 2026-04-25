//@ runDefault("--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true", "--useConcurrentJIT=0", "--watchdog=500", "--watchdog-exception-ok")

for (;;) {
    (() => {
        function f() {
            return undefined;
        }
        try { f(); } catch (e) {}
        const name = f.name;
        try { name.isWellFormed(); } catch (e) {}
        name.__proto__.replace(name, name);
        for (let i = 0; i < 100; i++) {
        }
    })()
}
