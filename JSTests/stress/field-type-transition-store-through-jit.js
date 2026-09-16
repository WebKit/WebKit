//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A creation store from JIT-compiled code that violates a claim established by earlier creations. Two readers so
// the claim is armed by one compilation and consumed by a later one.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.x = 999; this.b = 2; }
function mk(v) { const o = {}; o.f = v; return o; }
function read1(h) { return h.f.a; }
function read2(h) { return h.f.a; }
noInline(mk); noInline(read1); noInline(read2);

const hs = [];
for (let i = 0; i < 64; ++i) hs.push(mk(new A()));
for (let i = 0; i < testLoopCount; ++i) read1(hs[i & 63]);
for (let i = 0; i < testLoopCount; ++i) read2(hs[i & 63]);
shouldBe(read2(hs[0]), 111);

// Creating stores that violate the claim, performed by the now-compiled mk().
shouldBe(read2(mk(new B())), undefined);
shouldBe(read2(mk(new B())), undefined);
