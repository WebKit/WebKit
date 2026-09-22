//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A replace store from JIT-compiled code that violates the claim, which the inline check in the generated handler
// must catch. See field-type-store-side-maintenance.js for the C++ channels this one cannot isolate.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.x = 999; this.b = 2; }
function Holder(x) { this.f = x; }
function write(h, v) { h.f = v; }
function read(h) { return h.f.a; }
noInline(write); noInline(read);

const hs = [];
for (let i = 0; i < 64; ++i) hs.push(new Holder(new A()));
for (let i = 0; i < testLoopCount; ++i) { write(hs[i & 63], new A()); read(hs[i & 63]); }
shouldBe(read(hs[0]), 111);

write(hs[0], new B());
shouldBe(read(hs[0]), undefined);
write(hs[1], new B());
shouldBe(read(hs[1]), undefined);
