//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// A NON-NUMBER stored into a slot the raw-double mask claims is a TYPE CONFUSION, and this is its regression test.
//
// StructureTransitionTable::Hash::Key deliberately excludes PropertyAttribute::RepresentationDouble, so `o.a = 1.5`
// and `o.a = "s"` compute the SAME transition key and whichever store creates the transition decides the
// representation. The documented invariant covers two of the three follow-up cases: an Int32 into a raw slot coerces
// losslessly, and a double into a boxed slot is the pre-existing NaN-boxed behaviour. The third -- a non-number into a
// RAW slot -- cannot be represented at all and must widen the representation first.
//
// It did not. JSObject::widenDoubleRepresentation only fires for a property the object ALREADY has (it returns false
// on invalidOffset), so the ADD path went straight to the cached raw transition and stored the JSValue boxed. Because
// the mask still said raw, every reader de-biased it and handed the CELL POINTER back to JavaScript as a double:
//
//     for (i = 0; i < 2000; ++i) mk(1.5);   // establishes `a` as raw
//     mk("leak me")                          // -> 2.279749513e-314, i.e. the JSString* (0x115081600)
//     mk({})                                 // -> the JSObject* (0x115803e60)
//
// That is an address disclosure and a type confusion, and it is worse than a wrong number: GC tracing consults the
// same mask, so the live cell sitting in that slot is never traced.
//
// The fix widens the transition TARGET on the add path (JSObjectInlines.h putDirectInternal): the attribute-change
// sibling that does not claim the slot is used instead, which leaves every object already on the raw structure alone.
// JSObject::putDirectOffsetRawDoubleAware now also RELEASE_ASSERTs rather than storing boxed, so any path that still
// reaches it fails closed instead of mis-typing a pointer.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: got " + actual + " expected " + expected);
}

function shouldBeType(actual, expected) {
    if (typeof actual !== expected)
        throw new Error("bad type: got " + typeof actual + " (" + actual + ") expected " + expected);
}

// Fresh shape per call, so the property is ADDED (not replaced) every time -- that is the path that lacked widening.
function store(v) {
    const o = {};
    o.a = v;
    return o.a;
}

// Establish `a` as a raw double on this shape. 2000 iterations so every tier gets to install its inline caches.
for (let i = 0; i < 2000; ++i)
    shouldBe(store(1.5), 1.5);

// Each of these must come back with its own type and value, not as a double.
const string = "leak me";
shouldBeType(store(string), "string");
shouldBe(store(string), string);

const object = { marker: 1 };
shouldBeType(store(object), "object");
shouldBe(store(object), object);

shouldBeType(store(null), "object");
shouldBe(store(null), null);

shouldBeType(store(undefined), "undefined");
shouldBe(store(undefined), undefined);

shouldBeType(store(true), "boolean");
shouldBe(store(true), true);

shouldBeType(store(Symbol.iterator), "symbol");
shouldBe(store(Symbol.iterator), Symbol.iterator);

// Numbers must still round-trip exactly through the same shape, including the values whose bit patterns are the
// dangerous ones: 0.0 has raw bits 0x0, which read as a JSValue is EMPTY.
shouldBe(store(1.5), 1.5);
shouldBe(store(0), 0);
shouldBe(store(-0), -0);
shouldBe(1 / store(-0), -Infinity);
shouldBe(store(2000), 2000);
shouldBe(store(Infinity), Infinity);
shouldBe(store(NaN) !== store(NaN), true);

// The same sequence against a REPLACE rather than an add, which is the case widenDoubleRepresentation already covered.
function replaceOn(v) {
    const o = {};
    o.a = 1.5;
    o.a = v;
    return o.a;
}
for (let i = 0; i < 2000; ++i)
    shouldBe(replaceOn(2.5), 2.5);
shouldBeType(replaceOn("s"), "string");
shouldBeType(replaceOn({}), "object");
shouldBe(replaceOn(null), null);
shouldBe(replaceOn(7), 7);

// And through a constructor, which is the shape raytrace's classes produce.
function Holder(v) { this.a = v; }
for (let i = 0; i < 2000; ++i)
    shouldBe(new Holder(1.5).a, 1.5);
shouldBeType(new Holder("s").a, "string");
shouldBeType(new Holder({}).a, "object");
shouldBe(new Holder(null).a, null);
