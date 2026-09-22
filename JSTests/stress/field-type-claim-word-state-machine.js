//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Drives one field through every state of the shape-carried claim word, reading it through optimised code at each
// step: nothing recorded (consult the table), recorded but permanently generalised (skip), offset was reused
// (consult the table), and a live claim (compare against the value's structureID).
//
// Withdrawal must write the generalised state, not "nothing recorded": the latter is correct but sends every
// later creation of the shape back to the table, measured at -15.7% on json-parse-inspector.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function ShapeA(x) { this.x = x; }
function ShapeB(y) { this.y = y; this.x = -1; }   // different structure, still has `x`

function make(v) { const o = {}; o.f = v; return o; }
function readX(o) { return o.f.x; }
noInline(readX);
function readRaw(o) { return o.f; }
noInline(readRaw);

// Nothing recorded -> live claim: the first creation records ShapeA.
const a = [];
for (let i = 0; i < 64; ++i)
    a.push(make(new ShapeA(i)));
gc();
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readX(a[i & 63]), i & 63);

for (let i = 0; i < 256; ++i)
    a.push(make(new ShapeA(i)));
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i % a.length;
    shouldBe(readX(a[idx]), idx < 64 ? idx : (idx - 64) % 256);
}

// Live claim -> withdrawn: a creation of the same shape with a different structure.
const b = [];
for (let i = 0; i < 64; ++i)
    b.push(make(new ShapeB(i)));
gc();

for (let i = 0; i < testLoopCount; ++i) {
    const useA = (i & 1) === 0;
    const o = useA ? a[i % a.length] : b[i & 63];
    if (useA) {
        const idx = i % a.length;
        shouldBe(readX(o), idx < 64 ? idx : (idx - 64) % 256);
    } else {
        shouldBe(readX(o), -1);
        shouldBe(o.f.y, i & 63);
    }
}

// Withdrawn state, many more creations: these must skip cheaply and stay correct. Had the withdrawal written
// "nothing recorded", a later creation could re-establish a claim the earlier population contradicts.
for (let i = 0; i < 512; ++i) {
    const o = make((i & 1) ? new ShapeA(i) : new ShapeB(i));
    const v = readRaw(o);
    if ((i & 1) && v.x !== i)
        throw new Error("ShapeA mismatch at " + i);
    if (!(i & 1) && v.y !== i)
        throw new Error("ShapeB mismatch at " + i);
}
gc();

// Non-cell into the same field: still no claim, still correct.
for (let i = 0; i < 64; ++i)
    b[i].f = i + 0.5;
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readRaw(b[i & 63]), (i & 63) + 0.5);

for (let i = 0; i < 64; ++i)
    b[i].f = new ShapeA(i * 3);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readX(b[i & 63]), (i & 63) * 3);
