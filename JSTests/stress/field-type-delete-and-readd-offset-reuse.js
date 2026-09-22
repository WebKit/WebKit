//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// Deleting a field and adding another that recycles its offset must not let the old claim describe the new
// property. The deletion happens on a fresh object that never carried a claim; see
// field-type-offset-reuse-with-live-ancestor-claim.js for the live-claim case.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function read(o) { return o.f.a; }
noInline(read);

const hs = [];
for (let i = 0; i < 64; ++i) { const o = {}; o.f = new A(); hs.push(o); }
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);

// Delete f and add a differently-typed property, which may land on f's freed offset.
const victim = {};
victim.f = new A();
delete victim.f;
victim.g = 12345;
shouldBe(victim.g, 12345);
shouldBe(victim.f, undefined);
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
