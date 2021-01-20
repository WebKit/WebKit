//@ runDefault("--validateAbstractInterpreterState=1", "--validateAbstractInterpreterStateProbability=1.0", "--useRandomizingFuzzerAgent=1", "--thresholdForFTLOptimizeAfterWarmUp=0", "--useConcurrentJIT=0")

let x = 0;

function foo(a0) {
    for (let j=0; j<2; j++) {
        const y = a0 === typeof x;
        [a0, 0.1];
        x = [];
        ((y|0)**0) && 0;
    } 
}

for (let i = 0; i < 100; i++) {
    foo(0);
}
