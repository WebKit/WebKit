//@ runDefault("--useFTLJIT=0", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeSoon=100", "--collectContinuously=1", "--useGenerationalGC=0", "--numberOfGCMarkers=3", "--minimumMutatorUtilization=0.9")

// Stresses the DFG NewArray and MaterializeNewObject fast paths (both users of
// SpeculativeJIT::emitAllocateRawObject) against the concurrent collector. The publishing
// mutatorFence() used to be emitted inside emitAllocateRawObject(), i.e. before the caller's
// element-initializing stores, so on a weakly-ordered CPU the concurrent marker could observe
// publicLength = N with the [0, N) window of a recycled butterfly still uninitialized and walk
// stale doubles as JSValues. The reordering is stochastic, so this test cannot be made to fail
// deterministically on the unfixed build; it keeps the two lowerings compiled and hot while the
// collector marks concurrently, and asserts that every published array is well formed.

function makeContiguous(a, b, c, d, e) {
    return [a, b, c, d, e, {}, a, b, c, d, e, {}];
}
noInline(makeContiguous);

function makeDouble(a, b, c, d, e) {
    return [a + 0.5, b + 0.5, c + 0.5, d + 0.5, e + 0.5, a - 0.5, b - 0.5, c - 0.5];
}
noInline(makeDouble);

function makeInt32(a, b, c, d, e) {
    return [a | 0, b | 0, c | 0, d | 0, e | 0, (a + 1) | 0, (b + 1) | 0];
}
noInline(makeInt32);

// Object allocation sinking makes this materialize an object with a butterfly on the OSR-exit-free
// path, exercising compileMaterializeNewObject().
function makeSunkenObject(i, escape) {
    let o = { a: i + 1, b: i + 2, c: i + 3 };
    if (escape)
        return o;
    return o.a + o.b + o.c;
}
noInline(makeSunkenObject);

function check(condition, message) {
    if (!condition)
        throw new Error(message);
}

for (let i = 0; i < 200000; ++i) {
    let contiguous = makeContiguous(i, i + 1, i + 2, i + 3, i + 4);
    check(contiguous.length === 12, "bad contiguous length");
    check(contiguous[0] === i && contiguous[4] === i + 4, "bad contiguous elements");
    check(typeof contiguous[5] === "object", "bad contiguous cell element");

    let doubles = makeDouble(i, i + 1, i + 2, i + 3, i + 4);
    check(doubles.length === 8, "bad double length");
    check(doubles[0] === i + 0.5 && doubles[7] === i + 2 - 0.5, "bad double elements");

    let int32s = makeInt32(i, i + 1, i + 2, i + 3, i + 4);
    check(int32s.length === 7, "bad int32 length");
    check(int32s[0] === i && int32s[6] === i + 2, "bad int32 elements");

    let sunken = makeSunkenObject(i, (i % 1024) === 0);
    if (typeof sunken === "object")
        check(sunken.a === i + 1 && sunken.c === i + 3, "bad materialized object");
    else
        check(sunken === 3 * i + 6, "bad sunken object");
}
