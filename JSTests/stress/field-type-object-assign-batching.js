//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// Object.assign's batched property copies must maintain the claim, including the contradicting second assign.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.x = 999; this.b = 2; }
function read(o) { return o.f.a; }
noInline(read);

const hs = [];
for (let i = 0; i < 64; ++i) { const o = {}; o.f = new A(); hs.push(o); }
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);

const target = {};
Object.assign(target, { f: new A(), other: 1 });
shouldBe(read(target), 111);
Object.assign(target, { f: new B() });
shouldBe(read(target), undefined);
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
