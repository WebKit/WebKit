//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// Freezing an object carrying a claimed field must leave the claim usable for every other object of the shape.

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

const frozen = {};
frozen.f = new A();
Object.freeze(frozen);
shouldBe(read(frozen), 111);
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
