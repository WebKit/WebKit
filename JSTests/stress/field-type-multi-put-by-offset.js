//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A polymorphic store to a claimed field. S1 and S2 place the field at different offsets, so the claim may be
// declined in the default configuration; the eager no-cjit configurations narrow and exercise the per-variant
// store checks.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.x = 999; this.b = 2; }
function S1(x) { this.f = x; }
function S2(x) { this.pad = 1; this.f = x; }
function write(h, v) { h.f = v; }
function read(h) { return h.f.a; }
noInline(write); noInline(read);

const hs = [];
for (let i = 0; i < 64; ++i) hs.push(i & 1 ? new S1(new A()) : new S2(new A()));
for (let i = 0; i < testLoopCount; ++i) { write(hs[i & 63], new A()); read(hs[i & 63]); }
shouldBe(read(hs[0]), 111);

write(hs[0], new B());
shouldBe(read(hs[0]), undefined);
write(hs[1], new B());
shouldBe(read(hs[1]), undefined);
