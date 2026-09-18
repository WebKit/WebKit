//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A JSON.parse reviver replacing a field's value must be maintained like any other store.

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

const revived = JSON.parse('{"f":{"a":1}}', function (k, v) {
    if (k === "f") return new A();
    return v;
});
shouldBe(read(revived), 111);
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
