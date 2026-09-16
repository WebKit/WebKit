//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1", "--useDollarVM=1")

// Two structure-identity cases: a second global object has its own structures, so the same property name at the
// same offset belongs to a different owner and one realm's claim must never be applied to the other's objects;
// and an attribute change moves the object through Structure::attributeChangeTransition, which is not a
// PropertyAddition, so its transitionOffset is invalid and the claim stays owned by the ancestor that added the
// property -- stores through the changed structure must resolve that ancestor and withdraw its claim.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Cell(a) { this.a = a; }
function readF(o) { return o.f.a; }
noInline(readF);

const other = $vm.createGlobalObject();

// The same shape in both realms with different pointee structures, so confusing the two claims would specialise a
// load on the wrong structure.
const localHolders = [];
for (let i = 0; i < 64; ++i) {
    const o = {};
    o.f = new Cell(i);
    localHolders.push(o);
}

const foreignHolders = other.eval(`
    function ForeignCell(b) { this.b = b; this.a = b + 1000; }
    const out = [];
    for (let i = 0; i < 64; ++i) {
        const o = {};
        o.f = new ForeignCell(i);
        out.push(o);
    }
    out;
`);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readF(localHolders[i & 63]), i & 63);
gc();
for (let i = 0; i < 64; ++i)
    shouldBe(foreignHolders[i].f.a, i + 1000);

for (let i = 0; i < testLoopCount; ++i) {
    const o = (i & 1) ? localHolders[i & 63] : foreignHolders[i & 63];
    const v = readF(o);
    const expected = (i & 1) ? (i & 63) : (i & 63) + 1000;
    shouldBe(v, expected);
}
gc();

function makeHolder(v) { const o = {}; o.g = v; return o; }
function readG(o) { return o.g.a; }
noInline(readG);

const attrHolders = [];
for (let i = 0; i < 64; ++i)
    attrHolders.push(makeHolder(new Cell(i)));
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readG(attrHolders[i & 63]), i & 63);
gc();

// Change enumerability, which transitions each object to a new structure whose transitionOffset is invalid.
for (let i = 0; i < 32; ++i)
    Object.defineProperty(attrHolders[i], "g", { enumerable: false });

function Widened(a) { this.q = 1; this.a = a * 7; }
for (let i = 0; i < 32; ++i)
    attrHolders[i].g = new Widened(i);
gc();

for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    shouldBe(readG(attrHolders[idx]), idx < 32 ? idx * 7 : idx);
}
for (let i = 0; i < 32; ++i)
    shouldBe(Object.getOwnPropertyDescriptor(attrHolders[i], "g").enumerable, false);
for (let i = 32; i < 64; ++i)
    shouldBe(Object.getOwnPropertyDescriptor(attrHolders[i], "g").enumerable, true);
