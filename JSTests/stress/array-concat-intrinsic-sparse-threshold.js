// Tests that the ArrayConcatIntrinsic's fast path bails correctly for inputs that would
// require ArrayStorage: the DFG operation returns empty, the JIT speculation-checks against
// that and OSR-exits with ExoticObjectMode. After enough exits, subsequent compilations of
// the same call site see the exit profile and decline the intrinsic, so execution proceeds
// through the host function (which correctly handles large-size concat).

function assert(cond, msg) {
    if (!cond)
        throw new Error("Bad: " + (msg || ""));
}
noInline(assert);

function makeInt32(n) {
    const a = [];
    for (let i = 0; i < n; i++)
        a.push(i | 0);
    return a;
}
noInline(makeInt32);

function doConcat(a, b) { return a.concat(b); }
noInline(doConcat);

for (let i = 0; i < testLoopCount; i++)
    doConcat(makeInt32(2), makeInt32(2));

{
    const a = makeInt32(60000);
    const b = makeInt32(30000);
    const r = doConcat(a, b);
    assert(r.length === 90000, "under-threshold length " + r.length);
    assert(r[0] === 0 && r[59999] === 59999 && r[60000] === 0 && r[89999] === 29999,
           "under-threshold contents");
}

{
    const a = makeInt32(60000);
    const b = makeInt32(60000);
    for (let i = 0; i < 200; i++) {
        const r = doConcat(a, b);
        assert(r.length === 120000, "oversized length " + r.length);
        assert(r[0] === 0 && r[59999] === 59999 && r[60000] === 0 && r[119999] === 59999,
               "oversized contents");
    }
}

for (let i = 0; i < testLoopCount; i++) {
    const r = doConcat(makeInt32(2), makeInt32(3));
    assert(r.length === 5, "post-threshold small length " + r.length);
}
