//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A polymorphic read of a claimed field through MultiGetByOffset. A polymorphic base has no single offset owner
// in the default configuration, so no claim forms there; the eager no-cjit configurations do narrow. Either way
// the reads must be correct.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.b = 222; }
function S1(x) { this.f = x; }
function S2(x) { this.pad = 1; this.f = x; }
function read(h) { return h.f; }
noInline(read);

const hs = [];
for (let i = 0; i < 64; ++i) hs.push(i & 1 ? new S1(new A()) : new S2(new A()));
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]).a, 111);

// Now make one shape's field hold a B, so the two shapes disagree.
hs[0].f = new B();
shouldBe(read(hs[0]).a, undefined);
shouldBe(read(hs[0]).b, 222);
shouldBe(read(hs[1]).a, 111);
