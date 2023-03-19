//@ runDefault("--thresholdForOptimizeAfterWarmUp=100", "--useConcurrentJIT=0")
let obj = [];

for (let i = 0; i < 10; i++) {
    let [a] = obj;
    obj.length = 1;
    for (let j = 0; j < 10000; j++) { }
}
