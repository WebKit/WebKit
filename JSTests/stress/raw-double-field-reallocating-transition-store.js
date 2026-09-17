//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--useDFGJIT=0", "--useFTLJIT=0", "--thresholdForOptimizeAfterWarmUp=50")
//
// A raw double field added by a TRANSITION THAT REALLOCATES THE BUTTERFLY.
//
// The baseline inline cache serves that case with putByIdTransitionReallocatingOutOfLineHandler, which does not store
// inline at all -- it calls operationReallocateButterflyAndTransition. That operation did:
//
//     baseObject->nukeStructureAndSetButterfly(vm, StructureID::encode(oldStructure), newButterfly);
//     baseObject->putDirectOffset(vm, offset, JSValue::decode(encodedValue));   // <-- bare overload
//     baseObject->setStructure(vm, newStructure);
//
// The bare putDirectOffset consults this->structure(), which between those two lines is still OLD structure -- the one
// that owns neither the new offset nor its raw-double mask bit. So it stored a NaN-boxed double into a slot that
// newStructure, installed on the very next line, claims is raw. Every reader then added 2^49.
//
// Found on Octane raytrace: Plane.d went in as 1.2 and read back as 1.325, which is double(bits(1.2) + 2^49). It
// required the field to land on the FIRST OUT-OF-LINE OFFSET (firstOutOfLineOffset == 64) with reallocation, which is
// why exactly one field in that benchmark was wrong and Sphere.radius was not. operationPutByMegamorphicReallocating
// had the identical defect.
//
// The test therefore has to push a double field past inline capacity. Inline capacity is inferred per shape, so it
// sweeps a range of preceding-property counts rather than assuming one.

const f64 = new Float64Array(1);
const u32 = new Uint32Array(f64.buffer);
function bits(d) {
    f64[0] = d;
    return (u32[1] >>> 0).toString(16).padStart(8, "0") + (u32[0] >>> 0).toString(16).padStart(8, "0");
}
function shouldBeBits(what, actual, expected) {
    if (bits(actual) !== bits(expected))
        throw new Error(what + ": got " + actual + " (" + bits(actual) + ") expected " + expected + " (" + bits(expected) + ")");
}

const NAMES = [];
for (let i = 0; i < 24; ++i)
    NAMES.push("p" + i);

// `leading` Int32 properties (which stay boxed) and then one double, so the double is the property that spills.
function addThenRead(leading, value, withIndexedHeader) {
    const o = {};
    if (withIndexedHeader)
        o[0] = 1; // forces an indexing header, which is what selects the ReallocatingOutOfLine handler
    for (let i = 0; i < leading; ++i)
        o[NAMES[i]] = i;
    o.tail = value;
    return o.tail;
}

// 1.2 specifically: it is the raytrace value, and bits(1.2) + 2^49 is 1.325, a plausible-looking double that no
// value-range check would flag.
const VALUES = [1.2, 1.5, -1.5, 0.1, 1 / 3, 2000, Math.PI, 5e-324, 1.7976931348623157e308, -0.0];

for (const withIndexedHeader of [false, true]) {
    for (let leading = 0; leading < NAMES.length; ++leading) {
        for (const v of VALUES) {
            // Repeat so the site tiers up and the inline cache installs the reallocating transition handler.
            for (let i = 0; i < 200; ++i)
                shouldBeBits("leading=" + leading + " indexed=" + withIndexedHeader, addThenRead(leading, v, withIndexedHeader), v);
        }
    }
}

// The same shape, but reached through a class hierarchy, which is how raytrace produces it: a base constructor fills
// the inline slots and the subclass adds the double after super().
class Base {
    constructor() {
        this.a0 = 0; this.a1 = 1; this.a2 = 2; this.a3 = 3;
        this.a4 = 4; this.a5 = 5; this.a6 = 6; this.a7 = 7;
    }
}
class Derived extends Base {
    constructor(d) {
        super();
        this.d = d;
    }
}
for (const v of VALUES) {
    for (let i = 0; i < 2000; ++i)
        shouldBeBits("class hierarchy", new Derived(v).d, v);
}

// And a value that cannot live in a raw slot arriving at the same site, which must leave via the handler chain rather
// than reach the operation and be stored as raw bits.
for (let i = 0; i < 2000; ++i) {
    const asString = new Derived("not a number").d;
    if (asString !== "not a number")
        throw new Error("non-number through the reallocating transition: got " + asString);
    const asNull = new Derived(null).d;
    if (asNull !== null)
        throw new Error("null through the reallocating transition: got " + asNull);
}
