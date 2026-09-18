//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A claimed pointee gains a property, so its structure changes with nothing stored to the claimed field.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function Holder(x) { this.f = x; }
// Two readers, so the claim is armed by one compilation and consumed by a later one. With a single reader the
// claim is not yet established when the only compilation happens and the mechanism is never engaged.
function read1(h) { return h.f.a; }
function read2(h) { return h.f.a; }
noInline(read1);
noInline(read2);

const hs = [];
for (let i = 0; i < 64; ++i) hs.push(new Holder(new A()));
for (let i = 0; i < testLoopCount; ++i) read1(hs[i & 63]);
for (let i = 0; i < testLoopCount; ++i) read2(hs[i & 63]);
shouldBe(read2(hs[0]), 111);

// The field is never stored to, so only the dependency on the claimed structure's transition watchpoint can
// catch this.
hs[0].f.zzz = 5;
shouldBe(read2(hs[0]), 111);
shouldBe(hs[0].f.zzz, 5);
for (let i = 0; i < testLoopCount; ++i) shouldBe(read2(hs[1 + (i & 62)]), 111);
