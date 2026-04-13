//@ requireOptions("--validateDoesGC=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")

// rdar://172191300
// 308054@main marked CompareLess/LessEq/Greater/GreaterEq with HeapBigIntUse as
// doesGC()==false and added compileHeapBigIntCompare() for the non-peep-hole
// path, but forgot the peep-hole branch path, which fell through to
// genericJSValuePeepholeBranch() and called the generic operationCompareLess
// (with throw scope and exception check) under expectDoesGC==false.
//
// In a Debug build (ENABLE_DFG_DOES_GC_VALIDATION) this trips
// RELEASE_ASSERT(expectDoesGC()) when the watchdog/termination unwinds out of
// the JIT frame without resetting the DoesGC marker.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

// HeapBigInt operands (out of BigInt32 range so FixupPhase picks HeapBigIntUse).
var a = 2n ** 64n;
var b = a + 1n;

function lt(x, y)  { if (x < y)  return 1; return 0; }
function le(x, y)  { if (x <= y) return 1; return 0; }
function gt(x, y)  { if (x > y)  return 1; return 0; }
function ge(x, y)  { if (x >= y) return 1; return 0; }
function eq(x, y)  { if (x == y) return 1; return 0; }
noInline(lt); noInline(le); noInline(gt); noInline(ge); noInline(eq);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(lt(a, b), 1);
    shouldBe(le(a, b), 1);
    shouldBe(gt(a, b), 0);
    shouldBe(ge(a, b), 0);
    shouldBe(eq(a, a), 1);
}
