//@ requireOptions("--useConcurrentJIT=0", "--useFTLJIT=0", "--jitAllowList=test", "--jitPolicyScale=0.1")
// jsc ---useConcurrentJIT=0 -m --dumpDisassembly=1 --jitAllowList=test --jitPolicyScale=0.1 JSTests/stress/typed-array-byte-length.js

let arr = new Uint8Array(new ArrayBuffer(40));
let arr2 = new Uint8Array(new ArrayBuffer(80));

function test(a) {
    return a.byteLength
}

for (let i = 0; i < 1000; ++i) {
    if (test(arr) != 40)
        throw "A"
    if (test(arr2) != 80)
        throw "B"
}