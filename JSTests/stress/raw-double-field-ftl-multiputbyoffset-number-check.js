//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--useFTLJIT=1", "--thresholdForOptimizeAfterWarmUp=20", "--thresholdForOptimizeSoon=20", "--thresholdForFTLOptimizeAfterWarmUp=20", "--thresholdForFTLOptimizeSoon=20")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/08-repro-ftl-multiputbyoffset-fake-cell.js and the fix design in that directory's fixes/.
//
// REPRO for bug 08 — MultiPutByOffset gets NO Check(NumberUse), so the FTL stores an unchecked JSValue
// VERBATIM into a raw-double slot. An Int32 written that way is read back as a script-chosen JSCell*.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/08-repro-ftl-multiputbyoffset-fake-cell.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> ~390k wrong values, then SIGSEGV (rc=139)
//   --useRawDoubleFieldStorage=0                       -> "PASS"
//   --useFTLJIT=0                                      -> "PASS"
//   pre-patch build (WebKitBuild-without/Release)      -> "PASS"
//
// OBSERVED (staged tree, default options):
//   bad=393558  first: a.f=1.5 b.f=2.75 c.f=x
//   rc=139   -- the process dies on the deref, before printing anything after `bad=`
//
//   2.5 read back as 2.75 is exactly +DoubleEncodeOffset: the boxed double was written into the raw slot
//   and the raw reader biased it a second time.
//
// MECHANISM
//   DFGFixupPhase inserts the raw-slot type guard for PutByOffset only:
//
//       case PutByOffset: {
//           fixEdge<KnownCellUse>(node->child2());
//           if (!attemptToMakeDoubleRepForPut(node, node->child3())) {
//               if (node->storageAccessData().rawDoubleRep == RawDoubleRep::Raw) [[unlikely]] {
//                   m_insertionSet.insertNode(m_indexInBlock, SpecNone, Check, node->origin,
//                       Edge(node->child3().node(), NumberUse));      // DFGFixupPhase.cpp ~2738
//               }
//               speculateForBarrier(node->child3());
//           }
//
//   `case MultiPutByOffset` has no equivalent. Its data is a list of PutByVariants, each with its own
//   offset and its own destination structures, and nothing anywhere constrains the value edge.
//
//   FTLLowerDFGToB3::compileMultiPutByOffset then reaches the untyped arm (FTLLowerDFGToB3.cpp ~13195):
//
//       if (anyRaw) {
//           if (m_node->child2().useKind() == DoubleRepUse)
//               valueForVariant = unbiasedDoubleValue;
//           else
//               valueForVariant = rawDoubleBitsForStore(value, abstractValue(m_node->child2()).m_type,
//                                                       variant.offset());
//       }
//
//   and rawDoubleBitsForStore's last arm is, by its own comment, a fail-OPEN:
//
//       // A non-number needs the representation WIDENED, which a store cannot do on its own ...
//       // Counted rather than silently corrupted.
//       dataLogLnIf(Options::dumpDoubleFieldSplitCensus(), "[rawdouble] FTL store UNPROVEN-VALUE ...");
//       return jsValue;                               // <-- the JSValue goes into the raw slot as-is
//
//   "Counted rather than silently corrupted" is only true when the census option is on; with it off (the
//   default) the value is silently corrupted. Confirm the arm is taken with
//   `--dumpDoubleFieldSplitCensus=1` and grep for `UNPROVEN-VALUE` / `MULTIPUT`.
//
//   The monomorphic PutByOffset path is safe precisely because FixupPhase's Check makes the value provably
//   a number, so rawDoubleBitsForStore never reaches that arm. MultiPutByOffset skipped the Check and so
//   reaches it.
//
// WHY AN Int32 BECOMES A POINTER
//   box(int n) is NumberTag | n == 0xFFFE000000000000 | n. Stored verbatim into a raw slot and then read by
//   any raw-aware reader, which adds DoubleEncodeOffset (2^49):
//       0xFFFE000000000000 + 0x0002000000000000 = 0x0000000000000000  (wraps)
//   so the reader hands back JSValue(n). For any n with (n & NotCellMask) == 0 — i.e. any even n below 2^49
//   — isCell() is TRUE and the pointer is exactly n, a script-chosen constant. Below: 0x41414141.
//
// TRIGGER
//   Three shapes that put `f` at three different offsets, two of them RAW and one BOXED, all reaching one
//   `o.f = v` site. That makes PutByStatus multi-variant, so the DFG emits MultiPutByOffset rather than a
//   single PutByOffset, and the site must reach FTL.
//
// FIX SHAPE
//   Insert the same Check(NumberUse) for MultiPutByOffset whenever ANY variant's destination claims the
//   slot raw — DFGFixupPhase.cpp, alongside the PutByOffset case. Note DFGConstantFoldingPhase already
//   learned this lesson for the nodes it creates after FixupPhase has run (it inserts the Check itself);
//   MultiPutByOffset is the same gap in the other direction. Alternatively make rawDoubleBitsForStore fail
//   CLOSED like its C++ sibling putDirectOffsetRawDoubleAware does — but a RELEASE_ASSERT in FTL-generated
//   code is not an option, so the Check is the real fix.

function setter(o, v) { o.f = v; }
function reader(o) { return o.f; }
noInline(setter);
noInline(reader);

function makeA() { var o = {}; o.f = 0.5; return o; }                          // f at offset 0, RAW
function makeB() { var o = {}; o.b1 = 1; o.f = 0.5; return o; }                // f at offset 1, RAW
function makeC() { var o = {}; o.c1 = 1; o.c2 = 2; o.f = "str"; return o; }    // f at offset 2, BOXED
noInline(makeA);
noInline(makeB);
noInline(makeC);

var A = [], B = [], C = [];
for (var i = 0; i < 300; ++i) { A.push(makeA()); B.push(makeB()); C.push(makeC()); }

var bad = 0, firstBad = "";
for (var i = 0; i < 400000; ++i) {
    var a = A[i % 300], b = B[i % 300], c = C[i % 300];
    setter(a, 1.5);
    setter(b, 2.5);
    setter(c, "x");
    var ra = reader(a), rb = reader(b), rc = reader(c);
    if (ra !== 1.5 || rb !== 2.5 || rc !== "x") {
        bad++;
        if (!firstBad) firstBad = "a.f=" + ra + " b.f=" + rb + " c.f=" + rc;
    }
}

if (bad) {
    print("FAIL: " + bad + " wrong values out of 1200000 stores.  first: " + firstBad);
    print("      (2.5 -> 2.75 is exactly +DoubleEncodeOffset: box(d) written into a raw slot, then biased again)");
}

// The fake-cell half. Runs regardless, because the deref is the part that must not be reachable.
var victim = makeA();
setter(victim, 0x41414141);            // Int32; boxed form is 0xFFFE000041414141, stored verbatim
var v = reader(victim);                // raw reader adds 2^49 -> wraps to 0x41414141 -> isCell() true
print("reader(victim) typeof = " + typeof v + "  (expected number)");
print("about to dereference it");
print("deref: " + v.someProperty);     // SIGSEGV at 0x41414141 on the staged build
print("survived the deref");

if (bad)
    throw new Error(bad + " wrong values");
print("PASS");
