//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--forcePolyProto=true", "--useFTLJIT=1", "--thresholdForJITAfterWarmUp=10", "--thresholdForJITSoon=10", "--thresholdForOptimizeAfterWarmUp=20", "--thresholdForOptimizeSoon=20", "--thresholdForFTLOptimizeAfterWarmUp=20", "--thresholdForFTLOptimizeSoon=20")
//
// A PUT INLINE CACHE MAY NOT LEAVE THE CALLER'S VALUE REGISTER DE-BIASED.
//
// Storing into a raw slot needs bits(d), not box(d), and box(d) == bits(d) + 2^49, so the handlers subtracted
// DoubleEncodeOffset from valueJSR right before the store. They did it IN PLACE and never put it back. But valueJSR
// belongs to the CALLER: the FTL declares the value operand of a PutById patchpoint as a plain use, so B3/Air is free
// to materialise a loop-invariant boxed constant into a callee-saved register in the loop preheader and reuse that
// register on every iteration. Each trip through the handler subtracted 2^49 from it again:
//
//     <200> orr x22, xzr, #0x2000000000000   ; box(0.0), in the preheader -- executed ONCE
//     ...
//     sub x22, x22, #0x2000000000000         ; in the IC handler -- executed EVERY iteration
//
// so the constant decayed by one DoubleEncodeOffset per store. For 0.0 that is fatal immediately, because box(0.0) IS
// exactly DoubleEncodeOffset: one iteration takes the register to 0, which is JSValue::encode(JSValue()) -- the EMPTY
// value. The next store reached JSObject::putInlineForJSObject with an empty value ("ASSERTION FAILED: value",
// JSObjectInlines.h). On a release build the assertion is gone and the empty value is stored instead, which is worse.
//
// For 2.0 the same decay is slower and shows up as wrong numbers first: box(2.0) == 0x4002000000000000 == 0x2001 *
// 2^49, so it takes 8193 iterations to reach the empty value. That is why the failure looked value-independent and
// needed a long loop -- both variants are checked below.
//
// The generated-stub path (InlineCacheCompiler::generateAccessCase, adjustValueForRawDoubleStore) had the identical
// bug, and there it also corrupted the viaGlobalProxy write barrier, which tests branchIfNotCell on valueRegs after
// the store.
//
// Poly-proto is what keeps `green` at an offset the FTL will not fold into a direct PutByOffset, so the store stays a
// PutById with a real inline cache. The loop must be long enough to OSR-enter the FTL; run() is called once, so OSR
// entry is the only way its FTL code ever executes.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function makeCase(fillSource, fill) {
    // `if (!g)` makes g a phi rather than a plain argument, which is what puts the constant in a register that lives
    // across the loop instead of being rematerialised at each store.
    var C = new Function("g", "if (!g) g = " + fillSource + "; this.green = g;");
    return function () {
        // Warm with a genuine double so `green` is marked RepresentationDouble and the slot becomes raw.
        for (var i = 0; i < 3000; ++i)
            new C(0.5);

        var bad = 0;
        var first = null;
        for (var i = 0; i < 300000; ++i) {
            var v = new C(0).green;
            if (v !== fill) {
                if (first === null)
                    first = { iteration: i, value: v };
                ++bad;
            }
        }
        if (bad)
            throw new Error("stored " + fill + " but read back a wrong value " + bad + " times; first at iteration "
                + first.iteration + " was " + first.value);
    };
}

// 0.0 -- box(0.0) == DoubleEncodeOffset, so one clobber produces the empty JSValue.
noInline(makeCase);
var runZero = makeCase("0.0", 0.0);
noInline(runZero);
runZero();

// 2.0 -- decays through 8192 wrong-but-nonempty values before reaching empty.
var runTwo = makeCase("2.0", 2.0);
noInline(runTwo);
runTwo();

// And the plain non-poly-proto shape, to keep the pre-compiled Replace thunk covered: here the store lands on an
// object that already has the property, so it takes putByIdReplaceHandler rather than a transition handler.
function P(g) {
    this.green = g;
}
function replaceLoop() {
    var o = new P(0.5);
    for (var i = 0; i < 300000; ++i) {
        o.green = 0.0;
        if (o.green !== 0.0)
            throw new Error("replace handler corrupted the value register at iteration " + i + ": " + o.green);
    }
    return o.green;
}
noInline(replaceLoop);
shouldBe(replaceLoop(), 0.0);
