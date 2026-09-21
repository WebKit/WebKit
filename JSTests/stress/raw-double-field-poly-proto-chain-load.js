//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1", "--useDFGJIT=0", "--useFTLJIT=0")
//
// A raw double field held on a PROTOTYPE, read through a poly-proto access chain.
//
// AccessCase::structureOwningAccessedSlot() corrects two cases -- transitions report previousID(), and a prototype hit
// keeps the slot on the holder -- but it found the holder via tryGetAlternateBase(), which answers null whenever
// conditionSet() is empty. That is exactly the poly-proto case: the holder is not a constant object, it is reached by
// walking PolyProtoAccessChain::m_chain at run time. So the function fell through to structure(), i.e. the RECEIVER's
// structure, which does not own the slot -- the raw-double question was asked of the wrong Structure and answered
// false. The generated stub then emitted a bare load with no 2^49 reconstruction:
//
//     Load: {ident = 'uid:(a)', offset = 1, prototype access chain = PolyPolyProtoAccessChain: [...]}
//         ldur x0, [x2, #0x18]      <- the slot
//         retab                     <- returned unbiased
//
// so 1.5 read back as 1.375, 2000 as 1872, and -0.0 as a NaN -- each off by exactly one DoubleEncodeOffset in the
// mantissa. It needs THREE calls to show up: the site must go 3-way polymorphic before the IC gives up the pre-compiled
// Load thunk (which is raw-aware, it tests InlineCacheHandler::m_isRawDoubleField) and compiles a stub instead.
//
// DFG/FTL are off because this is about the inline-cache stub; the same fix covers them, since createPreCompiled and
// InlineCacheCompiler::generateAccessCase both consult structureOwningAccessedSlot().

const f64 = new Float64Array(1);
const u32 = new Uint32Array(f64.buffer);
function bits(d) {
    f64[0] = d;
    return (u32[1] >>> 0).toString(16).padStart(8, "0") + (u32[0] >>> 0).toString(16).padStart(8, "0");
}

// Compares BIT PATTERNS, not values: the bug moved the value by one DoubleEncodeOffset, and for -0.0 it produced a
// NaN, which `!==` alone would not catch cleanly.
function shouldBeBits(actual, expected) {
    if (bits(actual) !== bits(expected))
        throw new Error("bad bits: got " + actual + " (" + bits(actual) + ") expected " + expected + " (" + bits(expected) + ")");
}

// A fresh constructor each call gives a fresh prototype object, so the read site sees a new receiver structure every
// time and goes polymorphic. That is what forces the generated stub.
function readThroughPrototype(v) {
    function Base() { }
    Base.prototype.a = v;
    let last;
    for (let i = 0; i < 2000; ++i)
        last = new Base().a;
    return last;
}

const VALUES = [
    1.5, -1.5, 2000, 0.1, 1 / 3, -0.0, 0.0,
    Math.PI, Math.sqrt(2),
    5e-324, 2.2250738585072014e-308, 1.7976931348623157e308,
    4294967296, 2147483647, -2147483648, 1e21,
    Infinity, -Infinity,
];

// Three rounds: round 3 is the one that used to fail, so a regression that only shows up once the site is fully
// polymorphic is still caught.
for (let round = 0; round < 3; ++round) {
    for (const v of VALUES)
        shouldBeBits(readThroughPrototype(v), v);
}

// NaN separately -- the payload is not observable, only that it is still a NaN.
for (let round = 0; round < 3; ++round) {
    const got = readThroughPrototype(NaN);
    if (got === got)
        throw new Error("NaN read through the prototype chain came back as " + got);
}

// A prototype whose field is raw, shadowed by an own field that is not, and vice versa.
function shadowed(protoValue, ownValue) {
    function Base() { }
    Base.prototype.a = protoValue;
    let last;
    for (let i = 0; i < 2000; ++i) {
        const o = new Base();
        if (ownValue !== undefined)
            o.a = ownValue;
        last = o.a;
    }
    return last;
}
for (let round = 0; round < 3; ++round) {
    shouldBeBits(shadowed(1.5, 2.5), 2.5);
    shouldBeBits(shadowed(1.5, undefined), 1.5);
    shouldBeBits(shadowed(2000, 0.25), 0.25);
}
