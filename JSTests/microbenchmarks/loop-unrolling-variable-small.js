//@skip if $memoryLimited
//@requireOptions("--useFTLLoopUnrolling=1")
// jsc JSTests/microbenchmarks/loop-unrolling-*.js --useConcurrentJIT=0 --jitAllowList=main --useFTLLoopUnrolling=1 --loopFactorFTLLoopUnrolling=16 --dumpDisassembly=0

let arr = Array(1000).fill(0)

for (let i = 0; i < arr.length; ++i) {
    arr[i] = i;
}

function main(b, d) {
    "use strict";

    b = b | 0
    d = d | 0

    if (arr.length != 1000 || b < 0 || d < 0)
        return;

    let sum = 0

    for (let i = 0; i < d; ++i) {
        sum += arr[i] | 0
    }
    return sum
}
noInline(main)

const expected = 120

for (let i = 0; i < 100000000; ++i) {
    let r = main(5, 16)
    if (r != expected)
        throw "Error: expected " + expected + " got " + r
}
