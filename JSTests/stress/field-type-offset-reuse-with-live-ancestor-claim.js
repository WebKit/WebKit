//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Offset reuse while an ancestor still owns a live claim at that offset. Deletion and dictionary flattening
// renumber offsets, so a claim keyed on (ancestor, offset) can come to name a different property; the lazily-set
// "offset was reused" state of the claim word forces every later creation on such a shape onto the slow path that
// walks the ancestry instead of trusting the one-word fast path.
//
// field-type-delete-and-readd-offset-reuse.js does not reach this: it deletes from a fresh `{}` that never had a
// claim. Here the deletion happens on a shape that does carry a live claim at the recycled offset.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Cell(a) { this.a = a; }
function Other() { this.zzz = 1; this.a = 2; }   // deliberately a different structure than Cell

function makeHolder(v) { const o = {}; o.f = v; return o; }
function readF(o) { return o.f.a; }
noInline(readF);

const live = [];
for (let i = 0; i < 64; ++i)
    live.push(makeHolder(new Cell(5)));
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readF(live[i & 63]), 5);
gc();

// Delete `f` on separate objects of the same shape and add a different property, so the freed offset is recycled.
const recycled = [];
for (let i = 0; i < 32; ++i) {
    const o = makeHolder(new Cell(5));
    delete o.f;
    o.g = new Other();          // may land on f's freed offset
    o.h = i;
    recycled.push(o);
}
gc();

for (let i = 0; i < 32; ++i) {
    shouldBe(recycled[i].g.a, 2);
    shouldBe(recycled[i].g.zzz, 1);
    shouldBe(recycled[i].h, i);
    shouldBe(recycled[i].f, undefined);
}

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readF(live[i & 63]), 5);

// Re-add `f` with a different structure than the claim names: were the claim applied through the reused offset, a
// load specialised on Cell would misread an Other.
for (let i = 0; i < 32; ++i)
    recycled[i].f = new Other();
gc();
function readFLoose(o) { return o.f.a; }
noInline(readFLoose);
for (let i = 0; i < testLoopCount; ++i) {
    const o = recycled[i & 31];
    shouldBe(readFLoose(o), 2);
    shouldBe(o.g.zzz, 1);
}

const mixed = live.concat(recycled);
for (let i = 0; i < testLoopCount; ++i) {
    const o = mixed[i % mixed.length];
    const v = o.f.a;
    if (v !== 5 && v !== 2)
        throw new Error("unexpected f.a = " + v);
}
