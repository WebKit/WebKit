//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// Basic narrowing: a monomorphic field claims its pointee's structure, and reads through it stay correct.
// --validateFieldTypes is what gives every test in this suite teeth. Value checks alone pass even with a false
// claim sitting in the heap; the verifier turns a violated claim into a test failure.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function Holder(x) { this.f = x; }
function read(h) { return h.f.a; }
noInline(read);

const hs = [];
for (let i = 0; i < 64; ++i) hs.push(new Holder(new A()));
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
shouldBe(read(hs[0]), 111);
