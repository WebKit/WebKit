//@ runDefault("--thresholdForFTLOptimizeAfterWarmUp=1000", "--useConcurrentJIT=0")
for (let i = 0; i < 1000; ++i) {
    (() => {
        for (let v8 = 0; v8 < 25; v8++) {}
        const v10 = [...[1,2,3,4,5]];
        v10[2] = 3;
        function f11(a12) { return a12;}
        class C13 extends f11 {}
        v10.at(3745);
    })();
}
