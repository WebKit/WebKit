//@ requireOptions("--useConcurrentJIT=0", "--useFTLJIT=0", "--jitAllowList=test", "--jitPolicyScale=0.1")
// jsc --useConcurrentJIT=0 -m --dumpDisassembly=1 --jitAllowList=test --jitPolicyScale=0.1 JSTests/stress/put-by-id-megamorphic-reallocating-with-guard.js

function test(o, i) {
    o.f = i
}

for (var i = 0; i < 100; ++i) {
    let array = []
    for (let i = 0; i < 10; ++i) {
        var o = {};
        o["i" + i] = i;
        o["j" + i] = i;
        o["k" + i] = i;
        o["l" + i] = i;
        o["m" + i] = i;
        o["n" + i] = i;
        o["o" + i] = i;
        o["p" + i] = i;
        o["q" + i] = i;
        o["r" + i] = i;
        array.push(o);
    }
    let instance = array[i % array.length];
    test(instance, i)
    if (instance.f != i)
        throw "A"
}