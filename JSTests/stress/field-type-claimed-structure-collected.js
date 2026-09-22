//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Weak treatment of the claimed structure. A record stores a raw StructureID and keeps nothing alive, so once
// that structure is collected the ID can be recycled and StructureID::decode() hands the compiler an unrelated
// Structure*. Records naming dead structures must therefore be cleared at the end of marking, before anything is
// swept and so before any ID can be recycled.

function readField(h) { return h.f; }
function readThrough(h) { return h.f.x; }
noInline(readField);
noInline(readThrough);

// Each round claims a fresh structure for the same field, then drops every reference to it, so the claimed
// structure becomes garbage while the record still names it.
function makeRound(round) {
    const Pointee = new Function("this.x = 1; this.tag" + round + " = " + round + ";");
    function Holder(p) { this.f = p; }
    const holders = [];
    for (let i = 0; i < 32; ++i)
        holders.push(new Holder(new Pointee()));
    for (let i = 0; i < 1000; ++i) {
        if (readThrough(holders[i & 31]) !== 1)
            throw new Error("bad read in round " + round);
    }
    return holders.length;
}

let total = 0;
for (let round = 0; round < 24; ++round) {
    total += makeRound(round);
    // Collect the previous round's structures while records still name them.
    gc();
}
if (total !== 24 * 32)
    throw new Error("unexpected total " + total);

function Cell(x) { this.x = x; }
function Holder2(p) { this.f = p; }
const live = [];
for (let i = 0; i < 64; ++i)
    live.push(new Holder2(new Cell(i)));
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    if (readThrough(live[idx]) !== idx)
        throw new Error("expected " + idx + ", got " + readThrough(live[idx]));
}
gc();
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    const v = readField(live[idx]);
    if (!(v instanceof Cell) || v.x !== idx)
        throw new Error("bad field after gc at " + idx);
}
