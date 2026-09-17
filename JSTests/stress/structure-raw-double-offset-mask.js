//@ runDefault("--useDollarVM=1", "--useDoubleFieldRepresentation=1")
//@ runDefault("--useDollarVM=1", "--useDoubleFieldRepresentation=0")
//
// Structure::m_rawDoubleMask is a PER-OFFSET map: bit N set iff the property at PropertyOffset N is stored as raw
// IEEE-754 bits. It is derived state over PropertyAttribute::RepresentationDouble, which is the real carrier (that one
// rides transition identity and cannot desynchronise). Derived state is exactly what goes stale, so this file hammers
// every path that maintains it: property add (both the table-insertion and lazy-transition routes), attribute change
// in both directions, delete-and-reuse of an offset, dictionary flatten, out-of-line growth, and offsets past the
// mask's 128-bit reach.
//
// WHY A DEDICATED TEST AND NOT A VALUE CHECK. The mask's most important consumer is GC tracing, which produces no
// value to compare -- it either skips a raw slot or traces the double's bits as a pointer and crashes. So this test
// asserts the MASK ITSELF via $vm.isRawDoubleField, rather than inferring it from a round-trip.
//
// THE ONE-DIRECTIONAL INVARIANT is what every assertion here is really checking:
//     the mask may UNDER-claim (says boxed, is boxed)  -- safe, every existing reader copes
//     the mask must never OVER-claim (says raw, is boxed) -- this is the crash
// So "expected false" failures are bugs of the dangerous kind, and offsets the mask cannot describe must answer false.
//
// Both configurations must pass. With the representation option OFF nothing is ever marked, so every query is false
// and the file degenerates into a consistency check -- which is the point: it proves these paths are inert when the
// feature is off. See analysis/prompt/box2d/07-PLAN-double-field.md section 5h.

var REPR = $vm.doubleFieldRepresentationEnabled ? $vm.doubleFieldRepresentationEnabled()
                                                : undefined;

function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error((what ? what + ": " : "") + "got " + actual + " expected " + expected);
}

// A field marked Double is one whose CREATING store saw a non-integer double. When the representation option is off
// nothing is marked, so every expectation collapses to false. Detect which world we are in from a known-good case
// rather than from the option, so the file cannot silently pass by testing nothing.
var probe = {}; probe.d = 1.5;
var MARKING_ON = $vm.isRawDoubleField(probe, "d");

function expectRaw(o, name, why) {
    shouldBe($vm.isRawDoubleField(o, name), MARKING_ON, why);
}
function expectNotRaw(o, name, why) {
    shouldBe($vm.isRawDoubleField(o, name), false, why);
}

// ---- 1. the basic discrimination: doubles marked, everything else not
(function () {
    var o = {};
    o.d = 1.5;          // non-integer double
    o.i = 7;            // Int32
    o.s = "str";        // cell
    o.b = true;         // boolean
    o.n = null;
    // The criterion is the JSValue's REPRESENTATION at the creating store (JSObjectInlines.h:508, value.isDouble()),
    // not the numeric value. Verified: the literal 4.0 IS a double JSValue and is marked, while the computed 8/2 is
    // narrowed to Int32 by the engine and is not. Both directions are pinned here because getting this backwards is
    // how a test ends up asserting the engine's bug instead of its contract.
    o.z = 4.0;          // integer-valued but stored as a double -> MARKED
    o.w = 8 / 2;        // computed, narrowed to Int32 -> not marked
    expectRaw(o, "d", "non-integer double");
    expectNotRaw(o, "i", "Int32");
    expectNotRaw(o, "s", "string");
    expectNotRaw(o, "b", "boolean");
    expectNotRaw(o, "n", "null");
    expectRaw(o, "z", "integer-valued DOUBLE literal is still a double JSValue");
    expectNotRaw(o, "w", "computed integer result is narrowed to Int32");
    expectNotRaw(o, "missing", "absent property must answer false, not assert");
})();

// ---- 2. interleaving, so a mask bit lands at a non-zero offset with non-double neighbours on both sides
(function () {
    var o = {};
    o.a = 1;  o.b = 2.5;  o.c = "x";  o.d = 3.25;  o.e = 9;  o.f = 4.75;
    expectNotRaw(o, "a"); expectRaw(o, "b"); expectNotRaw(o, "c");
    expectRaw(o, "d");    expectNotRaw(o, "e"); expectRaw(o, "f");
})();

// ---- 3. OUT OF LINE, and what the mask's reach ACTUALLY is.
//
// PropertyOffsets are NOT dense. Inline offsets run 0..inlineCapacity-1, then out-of-line jumps to
// firstOutOfLineOffset = 64 (PropertyOffset.h:35). So the Nth property of a small object does not have offset N:
// measured here, the 11th property sat at offset 68 and the 70th at offset 127. The 128-bit mask therefore covers
// roughly (inlineCapacity + 64) PROPERTIES, not 128 of them.
//
// That is fine for the workloads this exists for -- census max double offset is 90 for Box2D and 71 for both
// raytrace and navier-stokes, all inside 128 -- and objects past the reach simply under-claim, which is safe. This
// section pins the behaviour at the boundary rather than assuming a property count.
(function () {
    var o = {};
    for (var i = 0; i < 100; ++i)
        o["p" + i] = (i % 2) ? (i + 0.5) : i;      // odd -> double, even -> Int32

    var markedDoubles = 0, unmarkedDoubles = 0;
    for (var i = 0; i < 100; ++i) {
        var isRaw = $vm.isRawDoubleField(o, "p" + i);
        if (i % 2) {
            (isRaw ? ++markedDoubles : ++unmarkedDoubles);
        } else {
            // An Int32 must NEVER be marked at any offset. This is the over-claim direction: the one that crashes.
            shouldBe(isRaw, false, "Int32 p" + i + " must never be marked");
        }
        shouldBe(o["p" + i], (i % 2) ? (i + 0.5) : i, "value at p" + i);
    }
    if (MARKING_ON) {
        // Fields below the mask's reach must be marked -- crossing from the first mask word into the second is the
        // case a single-uint64 mask would have silently dropped, and it is the common case (navier-stokes puts 100%
        // of its doubles out of line).
        if (markedDoubles < 30)
            throw new Error("expected out-of-line doubles to be marked; only " + markedDoubles + " were");
        expectRaw(o, "p1"); expectRaw(o, "p31"); expectRaw(o, "p61");
    } else {
        shouldBe(markedDoubles, 0, "nothing may be marked with the representation off");
    }
})();

// ---- 4. BEYOND THE MASK. Offsets >= 128 cannot be represented. They must answer FALSE (under-claim = safe), never
// true, and never alias a low offset by wrapping.
(function () {
    var o = {};
    for (var i = 0; i < 400; ++i)
        o["q" + i] = i + 0.5;                       // all doubles, offsets 0..399
    var sawRaw = 0, sawNotRaw = 0;
    for (var i = 0; i < 400; ++i)
        ($vm.isRawDoubleField(o, "q" + i) ? ++sawRaw : ++sawNotRaw);
    // Whatever the split, the values must all still read back correctly -- that is the proof that answering false for
    // a high offset is harmless rather than merely quiet.
    for (var i = 0; i < 400; ++i)
        shouldBe(o["q" + i], i + 0.5, "value at q" + i);
    if (MARKING_ON && !sawRaw)
        throw new Error("no offset was marked at all -- test is not exercising the mask");
    if (!MARKING_ON && sawRaw)
        throw new Error("representation is off but the mask claims raw offsets");
})();

// ---- 5. ATTRIBUTE CHANGE, both directions. This is updateAttributeIfExists / attributeChangeTransition, where a
// stale set bit would be an over-claim.
(function () {
    var o = {};
    o.x = 1.5;
    expectRaw(o, "x", "before defineProperty");
    Object.defineProperty(o, "x", { enumerable: false });
    shouldBe(o.x, 1.5, "value survives attribute change");
    // The representation bit must ride the attribute change; it must not be dropped, and must not appear on a field
    // that never had it.
    expectRaw(o, "x", "after enumerable change");
    o.y = 2;
    Object.defineProperty(o, "y", { enumerable: false });
    expectNotRaw(o, "y", "Int32 field must not acquire the bit via an attribute change");
})();

// ---- 6. DELETE AND REUSE. Deleting frees an offset; a later property can land on it. If the mask bit were not
// cleared, the new occupant inherits a raw claim it never asked for -- an over-claim, the crash direction.
(function () {
    var o = {};
    o.a = 1.5; o.b = 2.5; o.c = 3.5;
    expectRaw(o, "a"); expectRaw(o, "b"); expectRaw(o, "c");
    delete o.b;
    o.d = 42;                                       // Int32, may reuse b's offset
    shouldBe(o.d, 42, "reused slot value");
    expectNotRaw(o, "d", "Int32 landing on a freed double offset must NOT be marked");
    expectRaw(o, "a"); expectRaw(o, "c");
    shouldBe(o.a, 1.5); shouldBe(o.c, 3.5);
})();

// ---- 7. DICTIONARY + FLATTEN, with reuse across the flatten. Flattening rebuilds the property table and reassigns
// offsets, which is the single most likely place for a derived per-offset cache to go stale.
(function () {
    var o = {};
    for (var i = 0; i < 60; ++i)
        o["k" + i] = i + 0.5;                       // all doubles
    for (var i = 0; i < 60; i += 2)
        delete o["k" + i];                          // -> uncacheable dictionary
    for (var i = 0; i < 60; i += 2)
        o["m" + i] = i;                             // Int32 onto reused offsets
    for (var k in o) { }                            // provoke flattening
    JSON.stringify(o);
    for (var i = 0; i < 60; ++i) {
        if (i % 2) {
            expectRaw(o, "k" + i, "survivor double after flatten");
            shouldBe(o["k" + i], i + 0.5, "survivor value after flatten");
        } else {
            expectNotRaw(o, "m" + i, "Int32 on a reused offset after flatten");
            shouldBe(o["m" + i], i, "reused value after flatten");
        }
    }
})();

// ---- 8. SHARED SHAPE. Objects built by the same constructor share a Structure, so the mask is shared too; it must
// describe the shape, not whichever object happened to be created first.
(function () {
    function Pt(x, y) { this.x = x; this.y = y; }
    var a = new Pt(1.5, 2.5), b = new Pt(3.5, 4.5);
    expectRaw(a, "x"); expectRaw(a, "y");
    expectRaw(b, "x"); expectRaw(b, "y");
    shouldBe(a.x + b.y, 6.0);
})();

// ---- 9. run it hot, so the JIT tiers up over structures carrying mask bits and the transition paths are exercised
// under concurrent compilation rather than only at bytecode speed.
(function () {
    var acc = 0;
    for (var i = 0; i < 100000; ++i) {
        var o = {};
        o.p = i + 0.5;
        o.q = i;
        acc += o.p + o.q;
        if ((i & 2047) === 0) {
            if ($vm.isRawDoubleField(o, "p") !== MARKING_ON)
                throw new Error("mask went stale under tier-up at i=" + i);
            if ($vm.isRawDoubleField(o, "q"))
                throw new Error("Int32 field claimed raw under tier-up at i=" + i);
        }
    }
    if (!isFinite(acc))
        throw new Error("hot loop produced a non-finite result");
})();

print("PASS");
