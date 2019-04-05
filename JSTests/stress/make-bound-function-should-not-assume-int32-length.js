//@ runDefault("--useDoublePredictionFuzzerAgent=1", "--useConcurrentJIT=0")
// This test should not crash.
function assert(b) {
    if (!b)
        throw new Error("Bad")
}
noInline(assert);

function test(f, v, c, d) {
    return f.bind(v, c, d);
}

function foo(a,b,c,d,e,f) { return this; }
let thisValue = {};
for (let i = 0; i < 10000; i++) {
    let f = test(foo, thisValue, 20, 30);
    assert(f(foo, thisValue, 20, 30) === thisValue);
}
