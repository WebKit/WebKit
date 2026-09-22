//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// A store site compiled while no record existed for (owner, offset) emits no inline check, and JSC bakes the
// expected StructureID into the instruction stream, so that site can never learn about a claim established later.
// The fix is that an unchecked store site poisons the field permanently: there is nothing left to learn.

function Holder(v) { this.f = v; }
function Cell(x) { this.x = x; }

function store(h, v) { h.f = v; }
function read(h) { return h.f; }
noInline(store);
noInline(read);

// 1. Compile store() and read() while every f holds a non-cell, so no claim exists for the field when the store
//    site is compiled.
const warm = [];
for (let i = 0; i < testLoopCount; ++i) {
    const h = new Holder(i);
    store(h, i);
    read(h);
    if (warm.length < 64)
        warm.push(h);
}

// 2. Establish a cell-valued claim on the same (owner, offset), and get it consumed.
const holders = [];
for (let i = 0; i < 64; ++i)
    holders.push(new Holder(new Cell(i)));
for (let i = 0; i < testLoopCount; ++i)
    read(holders[i & 63]);

// 3. Write non-cells through the site compiled in step 1: if that site is unchecked and the claim survived, the
//    claim is now false.
for (let i = 0; i < 64; ++i)
    store(holders[i], i);

// 4. Every read must observe the number actually stored.
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    const v = read(holders[idx]);
    if (v !== idx)
        throw new Error("expected " + idx + ", got " + String(v));
}

// 5. The field must still work when cells come back.
for (let i = 0; i < 64; ++i)
    store(holders[i], new Cell(i + 1000));
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    const v = read(holders[idx]);
    if (!(v instanceof Cell) || v.x !== idx + 1000)
        throw new Error("expected Cell(" + (idx + 1000) + "), got " + String(v));
}
