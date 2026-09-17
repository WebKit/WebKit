//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// The BULK property writers -- JSON.parse's object fast path and putOwnDataPropertyBatching (Object.assign,
// CopyDataProperties) -- both look a transition up with attributes 0 and then store through a writer that does not
// consult the destination structure. Because StructureTransitionTable::Hash::Key excludes
// PropertyAttribute::RepresentationDouble, "attributes 0" can legitimately return a structure whose slot is RAW, and
// the boxed store then leaves a NaN-boxed double in a slot every reader de-biases.
//
// Two distinct failures were observed:
//
//   Object.assign: Math.PI copied out as 3.391592653589793, i.e. bits(PI) + one DoubleEncodeOffset. The write loop used
//   the bare putDirectOffset overload, which consults this->structure() -- still the OLD structure at that point, since
//   the stores all precede setStructure() -- so it asked a Structure that owns neither the offset nor its mask bit.
//
//   JSON.parse: a hard CRASH in JSObject::crashDueToEmptyValueAtValidOffset. A boxed Int32 0 is 0xfffe000000000000;
//   read as raw bits and biased by 2^49 it OVERFLOWS to exactly 0, which is the EMPTY JSValue. Reached by
//   JSON.parse(JSON.stringify({a: -0.0})) once the {a: <double>} transition already existed, because stringify maps
//   -0.0 to "0" and the parser then stores an Int32 into the raw slot's structure.
//
// LiteralParser now declines the fast path for a transition carrying a representation attribute, which is what the
// sibling single-transition check at LiteralParser.cpp had always done; the two were simply inconsistent.

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
function shouldBe(what, actual, expected) {
    if (actual !== expected)
        throw new Error(what + ": got " + actual + " expected " + expected);
}

const VALUES = [
    1.5, -1.5, 0.1, 1 / 3, 2000, Math.PI, Math.sqrt(2),
    4294967296, 2147483647, -2147483648, 1e21,
    5e-324, 2.2250738585072014e-308, 1.7976931348623157e308,
];

// Establish `a` as a raw double on the plain-object shape, so every bulk writer below meets a raw-claiming transition.
for (let i = 0; i < 2000; ++i) {
    const seed = {};
    seed.a = 1.5;
}

for (const v of VALUES) {
    for (let i = 0; i < 200; ++i) {
        shouldBeBits("Object.assign", Object.assign({}, { a: v }).a, v);
        shouldBeBits("spread", ({ ...{ a: v } }).a, v);
        shouldBeBits("JSON round trip", JSON.parse(JSON.stringify({ a: v })).a, v);
        shouldBeBits("JSON.parse literal", JSON.parse('{"a":' + v.toString() + "}").a, v);
    }
}

// The crash case: JSON.stringify turns -0.0 into "0", so the parser stores an Int32 whose boxed form is
// 0xfffe000000000000 -- the pattern that overflows to the empty JSValue when biased.
for (let i = 0; i < 2000; ++i) {
    shouldBe("JSON -0 round trip", JSON.parse(JSON.stringify({ a: -0.0 })).a, 0);
    shouldBe("JSON int into raw slot", JSON.parse('{"a":0}').a, 0);
    shouldBe("JSON zero via assign", Object.assign({}, { a: 0 }).a, 0);
}

// Bulk writers must also cope with values that cannot live in a raw slot at all: they have to stop batching and let the
// generic path widen, rather than storing a cell pointer where a double is expected.
for (let i = 0; i < 500; ++i) {
    shouldBe("assign string", Object.assign({}, { a: "s" }).a, "s");
    shouldBe("assign null", Object.assign({}, { a: null }).a, null);
    shouldBe("assign true", Object.assign({}, { a: true }).a, true);
    shouldBe("JSON string", JSON.parse('{"a":"s"}').a, "s");
    shouldBe("JSON null", JSON.parse('{"a":null}').a, null);
    shouldBe("JSON true", JSON.parse('{"a":true}').a, true);
    if (typeof Object.assign({}, { a: { n: 1 } }).a !== "object")
        throw new Error("assign object: got a " + typeof Object.assign({}, { a: { n: 1 } }).a);
}

// Multi-property batching, so the loop in putOwnDataPropertyBatching runs with a mix of representations and has to bail
// out part-way for the non-numeric member.
for (let i = 0; i < 500; ++i) {
    const seed = {};
    seed.p = 1.5; seed.q = 2.5; seed.r = 3.5;
    const copied = Object.assign({}, { p: 4.5, q: "not a number", r: 6.5 });
    shouldBeBits("batch p", copied.p, 4.5);
    shouldBe("batch q", copied.q, "not a number");
    shouldBeBits("batch r", copied.r, 6.5);
}

// Nested objects and arrays through JSON, which is where the parser's fast path is actually taken.
for (let i = 0; i < 500; ++i) {
    const parsed = JSON.parse('{"a":1.5,"b":{"a":2.5},"c":[{"a":3.5}]}');
    shouldBeBits("nested a", parsed.a, 1.5);
    shouldBeBits("nested b.a", parsed.b.a, 2.5);
    shouldBeBits("nested c[0].a", parsed.c[0].a, 3.5);
}
