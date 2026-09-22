//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")
// A claimed field must survive its owner becoming a dictionary and flattening again.

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

// Force one holder into a dictionary, then make it flatten again.
const dict = {};
dict.f = new A();
for (let i = 0; i < 200; ++i) dict["p" + i] = i;
for (let i = 0; i < 200; ++i) delete dict["p" + i];
shouldBe(read(dict), 111);
Object.keys(dict);
shouldBe(read(dict), 111);
for (let i = 0; i < testLoopCount; ++i) shouldBe(read(hs[i & 63]), 111);
