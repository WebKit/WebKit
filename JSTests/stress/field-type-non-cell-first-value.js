//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A field whose first value is not a cell must never be claimable, even once cells arrive later.
//
// Asserts the ABSENCE of a claim, so zero NARROW and non-zero GENERALIZE events are the correct outcome. Do not
// "fix" that by making the field monomorphic: a wrong claim here would corrupt the read.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function Holder() { this.f = null; }
function read(h) { return h.f === null ? -1 : h.f.a; }
noInline(read);

const hs = [];
for (let i = 0; i < 64; ++i) hs.push(new Holder());
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), -1);
for (let i = 0; i < 64; ++i) hs[i].f = new A();
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
