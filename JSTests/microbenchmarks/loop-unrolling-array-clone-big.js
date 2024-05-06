//@skip if $memoryLimited
//@requireOptions("--useFTLLoopUnrolling=1")
// jsc JSTests/microbenchmarks/loop-unrolling-*.js --useConcurrentJIT=0 --jitAllowList=main --useFTLLoopUnrolling=1 --loopFactorFTLLoopUnrolling=16 --dumpDisassembly=0

let arr = Array(20).fill(0)

for (let i = 0; i < arr.length; ++i) {
    arr[i] = i;
}

function main(arr, i) {
    "use strict";
    var copy = new Array(i);
    while (i--)
        copy[i] = arr[i];
    return copy;
}
noInline(main)

const expected = 49995000

for (let i = 0; i < 100000000; ++i) {
    let r = main(arr, 20)
    if (r.length != 20)
        throw "Bad length"
    for (let i = 0; i < r.length; ++i) {
        if (r[i] != i)
            throw "Error: expected " + i + " got " + r
    }
}
