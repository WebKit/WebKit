//@ runDefault("--useDollarVM=1")
//@ runDefault("--useDollarVM=1", "--useDoubleFieldRepresentation=1")
//
// Structure::hasRawDoubleFields is a summary bit (Structure.h, bit 31 of m_bitField): true if ANY property of the
// Structure carries PropertyAttribute::RepresentationDouble. It exists so a reader asking "is offset N raw-double?"
// can exit on one bit test instead of walking the property table.
//
// It is DERIVED state, so the risk is desynchronisation. This test hammers every path that could break it:
// property add, delete, re-add at a reused offset, attribute change, dictionary transition, flatten, and the generic
// readers (for-in, JSON, Object.keys/entries, getOwnPropertyDescriptor, spread).
//
// While nothing consumes the bit, the requirement is that behaviour is IDENTICAL with the representation option off
// and on. The harness runs this file both ways; any divergence or crash is the failure. It is deliberately written to
// be a pure output comparison so it keeps working once the bit does have consumers.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: got " + actual + " expected " + expected);
}

// ---- 1. doubles and non-doubles interleaved, so the bit is set partway through a transition chain
function mixedChain() {
    var o = {};
    o.i0 = 1;            // Int32 -- bit must NOT be set by this
    o.d0 = 1.5;          // double -- sets the bit on this transition
    o.s0 = "str";        // cell
    o.d1 = 2.25;
    o.i1 = 7;
    return [o.i0, o.d0, o.s0, o.d1, o.i1].join("|");
}
shouldBe(mixedChain(), "1|1.5|str|2.25|7");

// ---- 2. delete then re-add at a REUSED offset, mixing representations across the reuse
function offsetReuse() {
    var o = {};
    o.a = 1.5;           // double at some offset
    o.b = 2.5;
    delete o.a;          // offset freed; structure becomes uncacheable dictionary
    o.c = 42;            // Int32 may land on the offset the double used
    o.d = 3.5;
    var keys = Object.keys(o).join(",");
    return keys + " / " + [o.b, o.c, o.d].join("|") + " / " + (o.a === undefined);
}
shouldBe(offsetReuse(), "b,c,d / 2.5|42|3.5 / true");

// ---- 3. attribute change on a double-valued property (the updateAttributeIfExists path)
function attributeChange() {
    var o = {};
    o.x = 1.5;
    Object.defineProperty(o, "x", { enumerable: false });
    var d = Object.getOwnPropertyDescriptor(o, "x");
    return [o.x, d.enumerable, d.writable, Object.keys(o).length].join("|");
}
shouldBe(attributeChange(), "1.5|false|true|0");

// ---- 4. force a dictionary + flatten with many double fields, then read everything back
function flattenWithDoubles() {
    var o = {};
    for (var i = 0; i < 120; ++i)
        o["p" + i] = i + 0.5;          // all doubles
    for (var i = 0; i < 120; i += 2)
        delete o["p" + i];             // -> uncacheable dictionary
    for (var i = 0; i < 120; i += 2)
        o["q" + i] = i;                // Int32s onto reused offsets
    // touch it enough to provoke flattening, then verify every survivor
    var sum = 0, n = 0;
    for (var k in o) { sum += o[k]; ++n; }
    var viaJSON = JSON.parse(JSON.stringify(o));
    var mismatch = 0;
    for (var k in o) { if (viaJSON[k] !== o[k]) ++mismatch; }
    return [n, sum, mismatch, Object.keys(o).length].join("|");
}
// survivors: odd i as i+0.5 (60 terms, 60^2 + 60*0.5 = 3630) plus even i as Int32 (2*(0+..+59) = 3540)
shouldBe(flattenWithDoubles(), "120|7170|0|120");

// ---- 5. the generic readers, on an object that definitely has the bit set
function genericReaders() {
    var o = { a: 1.5, b: 2, c: "s", d: 4.25 };
    var parts = [];
    parts.push(Object.keys(o).join(","));
    parts.push(Object.values(o).join(","));
    parts.push(Object.entries(o).map(function (e) { return e[0] + "=" + e[1]; }).join(","));
    parts.push(JSON.stringify(o));
    parts.push(JSON.stringify(Object.assign({}, o)));
    parts.push(JSON.stringify({ ...o }));
    parts.push(String(Object.getOwnPropertyDescriptor(o, "a").value));
    var forIn = []; for (var k in o) forIn.push(k + ":" + o[k]);
    parts.push(forIn.join(","));
    return parts.join(" // ");
}
shouldBe(genericReaders(),
    'a,b,c,d // 1.5,2,s,4.25 // a=1.5,b=2,c=s,d=4.25 // {"a":1.5,"b":2,"c":"s","d":4.25} // '
    + '{"a":1.5,"b":2,"c":"s","d":4.25} // {"a":1.5,"b":2,"c":"s","d":4.25} // 1.5 // a:1.5,b:2,c:s,d:4.25');

// ---- 6. the same field taking BOTH an Int32 and a double over its lifetime.
// This is the population the whole project targets: 16.7% of stores to double fields carry an Int32.
function int32ThenDouble() {
    var out = [];
    for (var trial = 0; trial < 2; ++trial) {
        var o = {};
        if (trial === 0) { o.v = 0; o.v = 1.5; }   // Int32 first, then double
        else             { o.v = 1.5; o.v = 0; }   // double first, then Int32
        out.push(o.v + ":" + (o.v === Math.floor(o.v)));
    }
    return out.join("|");
}
shouldBe(int32ThenDouble(), "1.5:false|0:true");

// ---- 7. run it hot so the JIT tiers up over structures carrying the bit
function hot() {
    var acc = 0;
    for (var i = 0; i < 200000; ++i) {
        var o = {};
        o.x = i + 0.5;
        o.y = i;
        o.z = i + 0.25;
        acc += o.x + o.y + o.z;
        if ((i & 1023) === 0) { delete o.y; o.w = 1.5; acc += o.w; }
    }
    return acc;
}
if (!isFinite(hot()))
    throw new Error("hot loop produced a non-finite result");

// ---- 8. REGRESSION: get_by_val with a string key reaches JSCell::fastGetOwnProperty, the only caller routed
// through the raw-double-aware read. Marking a field Double while storage is still NaN-boxed once made this return
// bits(1.5)+2^49 reinterpreted as a double = 1.625, on whichever iterations took the C++ slow path -- an INTERMITTENT
// wrong answer that o.a (get_by_id, served by the IC) never showed. Keep this; it is the only coverage of that path.
function getByValStringKey() {
    var o = {}; o.a = 1.5; o.b = 2;
    var k = "a", sum = 0;
    for (var i = 0; i < 200000; ++i)
        sum += o[k];
    return sum / 200000;
}
shouldBe(getByValStringKey(), 1.5);

print("PASS");
