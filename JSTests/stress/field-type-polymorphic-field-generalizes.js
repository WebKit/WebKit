//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A field holding two structures must generalise rather than claim either.
//
// Asserts the ABSENCE of a claim, so zero NARROW and non-zero GENERALIZE events are the correct outcome. Do not
// "fix" that by making the field monomorphic: a wrong claim here would corrupt the read.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function A() { this.a = 111; }
function B() { this.b = 222; }
function Holder(x) { this.f = x; }
function readA(h) { return h.f.a; }
function readB(h) { return h.f.b; }
noInline(readA); noInline(readB);

const as = [], bs = [];
for (let i = 0; i < 32; ++i) { as.push(new Holder(new A())); bs.push(new Holder(new B())); }
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(readA(as[i & 31]), 111);
    shouldBe(readB(bs[i & 31]), 222);
}
