//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A sunk allocation materialised on the escaping path must not carry a stale claim on its field.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.x = 999; this.b = 2; }
function read(o) { return o.f.a; }
noInline(read);

function sink(v, escape) {
    const o = {};
    o.f = v;
    if (escape) return o;
    return o.f.a;
}
noInline(sink);

for (let i = 0; i < testLoopCount; ++i) shouldBe(sink(new A(), false), 111);
const escaped = sink(new A(), true);
shouldBe(read(escaped), 111);
const bad = sink(new B(), true);
shouldBe(read(bad), undefined);
