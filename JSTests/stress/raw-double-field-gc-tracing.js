//@ runDefault("--useDollarVM=1", "--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--useLLIntICs=0", "--useJIT=0")
//
// GC TRACING OVER RAW-DOUBLE SLOTS. The only test in this project whose failure mode is a CRASH rather than a wrong
// value, and therefore the only one that cannot be replaced by an assertion on a result.
//
// THE HAZARD. SlotVisitor::appendHiddenUnbarriered ends in `if (value.isCell()) trace(value.asCell())`, and isCell()
// is !(bits & NotCellMask) where NotCellMask == 0xffff000000000002. A raw IEEE-754 double whose bit pattern is below
// 2^48 therefore looks exactly like a cell pointer, and the collector follows it. That range is not exotic:
//
//     0.0                      bits 0x0000000000000000   <- the most common double in any program; traces as null
//     -0.0                     bits 0x8000000000000000   <- safe, but included as the sign-bit control
//     5e-324 (min subnormal)   bits 0x0000000000000001
//     1e-320                   bits 0x00000000000007e8
//     1.390671161566996e-309   bits 0x0000ffffffffffff   <- the largest colliding double
//
// Everything in [0, 1.390671161566996e-309] collides. Before Structure's raw-double mask was consulted by the two
// property-storage visit sites (JSObject.cpp, markAuxiliaryAndVisitOutOfLineProperties and
// JSFinalObject::visitChildrenImpl), this file's body killed the process:
//
//     MarkedBlock::aboutToMark(this=0x0000000000000000, cell=0x0000000000000001)   [thread 'Heap Helper Thread']
//     EXC_BAD_ACCESS (code=1, address=0x20)
//
// Mutation-proven: stubbing the isRawDoubleOffset consult to always-false brings the crash straight back (EXIT=139),
// so the check is load-bearing rather than incidentally passing.
//
// WHY --useLLIntICs=0 --useJIT=0, AND WHY THE FORCED FIELDS ARE READ ONLY AT THE END. Reading a raw slot through
// any of the OTHER reader families -- the LLInt get_by_id IC, op_enumerator_get_by_val, the JIT tiers -- is unbuilt
// work (07-PLAN section 5f-ter), and they crash for reasons that have nothing to do with the collector. Verified
// while writing this file: with them enabled it dies on the MAIN thread in LLInt loadConstantOrVariable at address
// 0x6, which is the raw bits of 1e-320 dereferenced by the interpreter. The GC-thread crash this file exists to
// prevent is a different stack entirely (MarkedBlock::aboutToMark on a Heap Helper Thread).
//
// So: disable the unbuilt readers, force, collect under pressure, and only then read back. DELETE those two flags
// once the tier readers are mask-aware; at that point this becomes an end-to-end test.
//
// See analysis/prompt/box2d/07-PLAN-double-field.md sections 5h and 5j.

function shouldBe(actual, expected, what) {
    if (!Object.is(actual, expected))
        throw new Error((what ? what + ": " : "") + "got " + actual + " expected " + expected);
}

// Every double whose raw bits fall in, or just outside, the cell-looking range.
var CASES = [
    0.0,                        // all-zero bits: traces as a null cell
    -0.0,                       // sign bit set: does NOT collide, control
    5e-324,                     // smallest subnormal
    1e-320,
    1e-310,
    6.95e-310,
    1.390671161566996e-309,     // largest colliding double
    1.5,                        // ordinary values: control
    -1.5,
    1e300,
];

var objs = [];
for (var c = 0; c < CASES.length; ++c) {
    var o = {};
    o.v = CASES[c];
    o.pad = 1.25;               // a second raw field, so the skip logic must handle runs, not just singletons
    if (!$vm.forceRawDoubleField(o, "v"))
        throw new Error("forceRawDoubleField failed for " + CASES[c] + " -- test cannot exercise the GC path");
    $vm.forceRawDoubleField(o, "pad");
    objs.push(o);
}

// OUT OF LINE, which is addressed in REVERSE from propertyStorage(): slot i within the traced range is offset
// firstOutOfLineOffset + outOfLineSize - i - 1. An off-by-one in that mapping skips the wrong slot and either
// crashes here or silently drops a real pointer from the mark, so cover it with a colliding value in the middle.
var big = {};
for (var i = 0; i < 90; ++i)
    big["f" + i] = (i === 80) ? 0.0 : (i + 0.5);
for (var i = 0; i < 90; ++i)
    $vm.forceRawDoubleField(big, "f" + i);
objs.push(big);

// A normal object graph alongside, so the collector has genuine pointers to follow and the test would notice if the
// skip logic dropped real cells instead of only raw doubles.
var keepAlive = [];
for (var i = 0; i < 200; ++i)
    keepAlive.push({ next: (i ? keepAlive[i - 1] : null), tag: "cell" + i, d: i + 0.5 });

for (var round = 0; round < 3; ++round) {
    for (var i = 0; i < 300; ++i) {
        var junk = [];
        for (var j = 0; j < 400; ++j)
            junk.push({ a: j, b: j + 0.5 });
    }
    $vm.gc();
}

// Only now read anything back.
for (var c = 0; c < CASES.length; ++c) {
    shouldBe(objs[c].v, CASES[c], "case " + c + " across GC");
    shouldBe(objs[c].pad, 1.25, "pad " + c + " across GC");
}
shouldBe(big.f80, 0.0, "out-of-line 0.0");
shouldBe(big.f89, 89.5, "out-of-line last slot");
shouldBe(big.f0, 0.5, "out-of-line first slot");

// The ordinary graph must be intact: nothing was dropped from the mark.
shouldBe(keepAlive.length, 200);
shouldBe(keepAlive[199].tag, "cell199");
var depth = 0;
for (var n = keepAlive[199]; n; n = n.next)
    ++depth;
shouldBe(depth, 200, "prototype chain of live cells survived");

print("PASS");
