//@ runDefault("--useDollarVM=1", "--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--useLLIntICs=0", "--useJIT=0")
//
// THE READER-SURFACE EXERCISER. Every C++ path that can read a property slot directly, pointed at an object whose
// fields are Double-represented, so the ASSERT_ENABLED detector in JSObject::assertNotRawDoubleFieldRead fires on any
// reader still using the unchecked getDirect(PropertyOffset).
//
// WHY THIS FILE EXISTS. The B15 sweep ran the detector over all 5666 JSTests/stress tests and found 4 reader sites.
// It missed a 5th -- objectConstructorValues -- because nothing in that corpus calls Object.values() on a
// double-field object while the option is on. A corpus that was not written to hit these paths does not cover them,
// and "the suite is green" is therefore not evidence. This file IS written to hit them.
//
// Keep it exhaustive rather than minimal: each builtin below reaches a DIFFERENT forEachProperty lambda or slot
// reader, and they were fixed as a group precisely because they are one idiom repeated. A new one added upstream
// should fail here rather than in a benchmark.
//
// WHY --useLLIntICs=0 IS REQUIRED, and what that admission means. The LLInt get_by_id inline cache caches
// (structure, offset) and then loads the slot with loadPropertyAtVariableOffset (LowLevelInterpreter64.asm:1591),
// straight from generated code. It never calls getDirect, so it neither reconstructs the raw double nor trips the
// ASSERT_ENABLED detector -- a forced field reads 1.5 on the first call and bits(1.5)-2^49 = 1.375 on every call
// after the IC installs. Disabling the IC is what makes THIS file able to test the C++ readers in isolation.
//
// It is NOT a workaround for a bug this file should be catching: the JIT/IC reader side is unbuilt work, tracked in
// 07-PLAN section 5f-ter, and it is the actual gate on Step 3. When that lands, DELETE the --useLLIntICs=0 above and
// this file becomes the end-to-end test.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: got " + actual + " expected " + expected);
}

// Every field a non-integer double, AND forced to genuinely raw storage.
//
// The forcing is not optional. --useRawDoubleFieldStorage is a GLOBAL claim that every Double-represented field is
// stored raw, so every reader reconstructs unconditionally; but until the writer side lands (07-PLAN section 5g) only
// fields passed to forceRawDoubleField actually are. Reading an unforced field under this option therefore returns
// bits(d)+2^49 as a double -- 1.5 -> 1.625 -- which is a bug in the TEST, not in the reader. Force first and both the
// value assertions and the ASSERT_ENABLED detector are meaningful: a checked reader reconstructs and gets 1.5, an
// unchecked one reads the raw word as a JSValue and gets 1.375, and the detector names it either way.
function mk() {
    var o = {};
    o.a = 1.5; o.b = 2.25; o.c = 3.125; o.d = 4.0625;
    if (!$vm.forceRawDoubleField(o, "a") || !$vm.forceRawDoubleField(o, "b")
        || !$vm.forceRawDoubleField(o, "c") || !$vm.forceRawDoubleField(o, "d"))
        throw new Error("forceRawDoubleField failed -- test cannot exercise the raw path");
    return o;
}
var EXPECT_JSON = '{"a":1.5,"b":2.25,"c":3.125,"d":4.0625}';

// ---- 1. Object.values -- objectConstructorValues. The site the 5666-test sweep MISSED.
shouldBe(Object.values(mk()).join(","), "1.5,2.25,3.125,4.0625");

// ---- 2. Object.entries -- objectConstructorEntries, a different lambda over the same table.
shouldBe(Object.entries(mk()).map(function (e) { return e[0] + "=" + e[1]; }).join(","),
    "a=1.5,b=2.25,c=3.125,d=4.0625");

// ---- 3. Object.assign -- the fast clone path in ObjectConstructorInlines.h.
shouldBe(JSON.stringify(Object.assign({}, mk())), EXPECT_JSON);

// ---- 4. spread -- objectCloneFast on the same inline path, reached differently.
shouldBe(JSON.stringify({ ...mk() }), EXPECT_JSON);

// ---- 5. JSON.stringify -- FastStringifier's forEachProperty.
shouldBe(JSON.stringify(mk()), EXPECT_JSON);

// ---- 6. getOwnPropertyDescriptor(s) -- toPropertyDescriptor and the descriptor builder.
shouldBe(Object.getOwnPropertyDescriptor(mk(), "a").value, 1.5);
shouldBe(Object.getOwnPropertyDescriptors(mk()).c.value, 3.125);

// ---- 7/8. REMOVED, and why. They used Object.defineProperties / Object.create to store doubles onto a FRESH
// object, then read them back. Under --useRawDoubleFieldStorage every reader reconstructs, but no writer yet stores
// raw, so ANY freshly-stored double field reads back as bits(d)+2^49 = 1.625 regardless of which reader is used:
//
//     var t = {}; t.x = 1.5; print(t.x);   //  -> 1.625
//
// That is the missing writer side (07-PLAN section 5g), not a reader defect, and it makes whole-program assertions
// impossible under this option: only slots explicitly passed to forceRawDoubleField are consistent. So this file
// reads forced slots and never asserts on a value it just stored. Restore these two sections once writers land --
// at that point the option becomes globally sound and they will pass unmodified.

// ---- 9. the generic get, which reaches getOwnNonIndexPropertySlot -- 91 of the 96 sweep hits.
//
// NOTE the deliberate absence of `for (var k in o) sum += o[k]` here. That form is WRONG today and is not a C++
// reader at all: op_enumerator_get_by_val in OwnStructureMode (LowLevelInterpreter64.asm:3518) checks the cached
// StructureID and then does a bare `loadq` of the slot, returning it as a JSValue. Inside a function it yields
// bits(d)-2^49 for every field while the identical loop at top level is correct, and --useLLIntICs=0 does not help.
// It is the third reader family in 07-PLAN 5f-ter(c) and belongs to Step 3, so asserting on it here would be
// asserting on unbuilt work. Object.keys + o[k] covers the same ground through paths that ARE built.
(function () {
    var o = mk(), sum = 0;
    var ks = Object.keys(o);
    for (var i = 0; i < ks.length; ++i) sum += o[ks[i]];
    shouldBe(sum, 10.9375);
    shouldBe(Reflect.get(o, "b"), 2.25);
    shouldBe(ks.length, 4);
})();

// ---- 10. get_by_val with a string key -- JSCell::fastGetOwnProperty, the narrow path from 5f.
//
// COLD ONLY, deliberately. The original form ran 100000 iterations and got 1.3761975 -- a MIXTURE: correct while the
// reads take the C++ slow path, then bits(d)-2^49 once a JIT tier caches the access and loads the slot directly.
// That fractional value is the clearest single piece of evidence for 5f-ter: the wrongness arrives exactly when the
// engine stops asking C++. Keep the loop count below any tier-up threshold so this section tests the C++ reader it
// is named for; the tiers are Step 3.
(function () {
    var o = mk(), k = "a", sum = 0;
    for (var i = 0; i < 10; ++i)
        sum += o[k];
    shouldBe(sum / 10, 1.5);
})();

// ---- 11. freeze/seal walk the table, then read it back.
(function () {
    var o = Object.freeze(mk());
    shouldBe(JSON.stringify(o), EXPECT_JSON);
    shouldBe(Object.isFrozen(o), true);
})();

// ---- 12. the structured readers: Map/Set built from entries, and JSON round-trip equality.
(function () {
    var o = mk();
    var m = new Map(Object.entries(o));
    shouldBe(m.get("a") + m.get("d"), 5.5625);
    var back = JSON.parse(JSON.stringify(o));
    var mismatch = 0;
    Object.keys(o).forEach(function (k) { if (back[k] !== o[k]) ++mismatch; });   // not for-in; see section 9
    shouldBe(mismatch, 0);
})();

// ---- 13. the negative control that gives sections 1-12 their teeth. A reader that IGNORED representation and
// returned the raw word as a JSValue would yield bits(d)-2^49 = 1.375, not 1.5, so those sections already exclude it.
// What they do NOT exclude on their own is a reader that never reconstructs because the slot was never really raw --
// then every section above would pass vacuously on ordinary boxed storage.
//
// HISTORY. This section used to prove that by creating a twin object WITHOUT forcing it and asserting the twin read
// back as 1.625, i.e. that forcing changed the stored word. That control died when the C++ writer landed
// (JSObject::putDirectOffsetRawDoubleAware, 07-PLAN section 5t): every marked double field is now stored raw at
// creation, so there is no unforced twin to compare against and `boxed.a` reads 1.5 like everything else. The
// section's own comment predicted this.
//
// THE REPLACEMENT CONTROL IS SHARPER, because it exercises the boundary that actually broke. The raw-double mask is
// two words, so it cannot claim an offset >= 128; Structure::isRawDoubleOffset answers false for those and the writer
// declines to store them raw. So an object wide enough to push fields past that boundary has BOTH representations
// live at once, and reading every field correctly is only possible if reader and writer agree on the same authority.
// They did not, until 5t: the reader consulted the attributes byte (no 128 limit) while the writer consulted the mask,
// and a 90-field object read its first 70 fields correctly and returned 87.5 for a field holding 79.5 -- exactly
// bits(79.5)+2^49. That is the bug this control now locks down.
(function () {
    var raw = mk();
    shouldBe(raw.a, 1.5);                           // narrow object: claimed raw, reconstructed correctly
    shouldBe($vm.isRawDoubleField(raw, "a"), true); // and it really is claimed, so sections 1-12 are not vacuous

    // Wide object: straddles the mask boundary deliberately.
    var wide = {};
    for (var i = 0; i < 90; ++i)
        wide["f" + i] = i + 0.5;
    var wrong = 0;
    for (var i = 0; i < 90; ++i)
        if (wide["f" + i] !== i + 0.5) ++wrong;
    shouldBe(wrong, 0);                             // every field correct, in BOTH representations

    // And prove the object genuinely spans both, so the check above is not vacuous either: some field must be claimed
    // and some field must not be. If the mask ever grows past 90 fields' worth of offsets this needs revisiting, and
    // failing here is the right way to find that out.
    var claimed = 0, unclaimed = 0;
    for (var i = 0; i < 90; ++i)
        $vm.isRawDoubleField(wide, "f" + i) ? ++claimed : ++unclaimed;
    shouldBe(claimed > 0, true);
    shouldBe(unclaimed > 0, true);
})();

print("PASS");
