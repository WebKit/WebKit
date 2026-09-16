//@ runDefault("--useDollarVM=1", "--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--useLLIntICs=0", "--useJIT=0")
//
// The ONLY coverage of the raw-double reconstruction in JSObject::getDirectRawDoubleAware.
//
// WHY THE JIT IS OFF. The C++ writer (JSObject::putDirectOffsetRawDoubleAware, 07-PLAN section 5t) stores marked
// fields raw, so this test no longer has to force anything -- ordinary property creation produces raw slots. But the
// LLInt inline cache, the baseline IC and the DFG all still store BOXED, so with any of them enabled a marked field's
// representation depends on which tier happened to perform the store. --useLLIntICs=0 --useJIT=0 pins every store to
// the C++ path so writer and reader are the only two participants. $vm.forceRawDoubleField is now a predicate that
// asserts the slot IS raw rather than making it so.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: got " + actual + " expected " + expected);
}

if (typeof $vm === "undefined" || typeof $vm.forceRawDoubleField !== "function")
    throw new Error("$vm.forceRawDoubleField unavailable; this test needs --useDollarVM=1");

var o = {};
o.a = 1.5;
o.b = 2;                                   // Int32 control, must be untouched
shouldBe($vm.forceRawDoubleField(o, "a"), true);
shouldBe($vm.forceRawDoubleField(o, "b"), false);   // not a marked double -> refused

// get_by_val with a string key is the one path routed through the raw-double-aware read.
var k = "a", kb = "b";
shouldBe(o[k], 1.5);
shouldBe(o[kb], 2);

// several distinct values, to prove it is a real reconstruction and not a lucky constant
var vals = [0.5, -2.25, 1e-300, 1e300, 0.1];
for (var i = 0; i < vals.length; ++i) {
    var t = {};
    t.v = vals[i];
    if (!$vm.forceRawDoubleField(t, "v"))
        throw new Error("force failed for " + vals[i]);
    var key = "v";
    shouldBe(t[key], vals[i]);
}

print("PASS");
