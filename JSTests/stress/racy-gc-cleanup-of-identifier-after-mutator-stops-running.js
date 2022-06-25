//@ skip if not $jitTests
//@ runDefault("--numberOfGCMarkers=1", "--useDFGJIT=false", "--useLLInt=false")

function foo() {
    gc();
    for (let i = 0; i < 100000; i++) {
        const o = { f: 42 };
        o[Symbol.split.description];
    }
}
function bar() {
    for (let i = 0; i < 100; i++)
        new Uint8Array(200000).subarray();
}

foo();
bar();
