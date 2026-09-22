//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A pre-existing object whose field disagrees must force the record to generalise.
//
// Asserts the ABSENCE of a claim, so zero NARROW and non-zero GENERALIZE events are the correct outcome. Do not
// "fix" that by making the field monomorphic: a wrong claim here would corrupt the read.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.x = 999; this.b = 2; }
function Holder(x) { this.f = x; }
function read(h) { return h.f.a; }
noInline(read);

const poisoned = new Holder(new B());
const clean = [];
for (let i = 0; i < 64; ++i) clean.push(new Holder(new A()));
for (let i = 0; i < testLoopCount; ++i) read(clean[i & 63]);
shouldBe(read(clean[0]), 111);
shouldBe(read(poisoned), undefined);
