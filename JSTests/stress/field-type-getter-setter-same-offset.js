//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// Replacing a claimed data field with an accessor at the same offset must not leave the claim describing it.

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

const acc = {};
acc.f = new A();
Object.defineProperty(acc, "f", { get() { return { a: 777 }; }, configurable: true });
shouldBe(read(acc), 777);
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
