//@ runDefault("--thresholdForJITAfterWarmUp=10", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--useConcurrentJIT=false")

'use strict';

function inlinee(value) {
    for (let i = 0; i < arguments.length; i++) {
    }
    let tmp = value + 1;
}

function reflect() {
    return inlinee.apply(undefined, arguments);
}

function test(arr) {
    let object = inlinee.apply(undefined, arr);
    reflect();
}

for (let i = 0; i < 10000; i++) {
    let arr = [];
    for (let j = 0; j < 1 + i % 100; j++)
        arr.push(1);
    test(arr);
}
