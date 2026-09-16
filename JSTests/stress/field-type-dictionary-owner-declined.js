//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// A cacheable-dictionary owner must never be recorded. addNewPropertyTransition can convert to a cacheable
// dictionary and add the property in place, so the owner handed to recordFieldTypeAtCreation can be a dictionary,
// which breaks the invariant behind the shape-carried claim: the claim word is per-structure, but a dictionary
// adds many properties to one structure, so the creation fast path could answer with a different offset's claim.
// Recording is declined at the source, since findOffsetOwner returns null for a dictionary and the verifier skips
// dictionaries, so nothing could consume such a record anyway.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Pointee(tag) { this.a = tag; }

// Enough distinct properties on one object to pass the cacheable-dictionary threshold, with cell-valued fields
// interleaved so that any wrongly recorded field would carry a claim.
function buildWide(tag) {
    const o = {};
    for (let i = 0; i < 128; ++i) {
        o["cell" + i] = new Pointee(tag + i);
        o["num" + i] = i;
    }
    return o;
}

const wide = [];
for (let round = 0; round < 4; ++round)
    wide.push(buildWide(round * 1000));

// Had a dictionary field been recorded, a load answered from a neighbouring offset's claim would be specialised on
// the wrong structure.
function readCell(o, k) { return o[k].a; }
noInline(readCell);

for (let i = 0; i < testLoopCount; ++i) {
    const round = i & 3;
    const idx = i % 128;
    shouldBe(readCell(wide[round], "cell" + idx), round * 1000 + idx);
}

// A claim owned by a normal ancestor, on an object that later becomes a dictionary.
function Holder(p) { this.f = p; }
function readThrough(h) { return h.f.a; }
noInline(readThrough);

const holders = [];
for (let i = 0; i < 64; ++i)
    holders.push(new Holder(new Pointee(7)));
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readThrough(holders[i & 63]), 7);

// Push one holder into dictionary territory, then write a differently-shaped value into its claimed field. The
// other 63 keep the pre-dictionary structure and its claim.
const dictHolder = holders[0];
for (let i = 0; i < 256; ++i)
    dictHolder["extra" + i] = i;
function OtherShape() { this.b = 1; this.a = 99; }
dictHolder.f = new OtherShape();
shouldBe(dictHolder.f.a, 99);

for (let i = 1; i < 64; ++i)
    shouldBe(readThrough(holders[i]), 7);
gc();
for (let i = 0; i < testLoopCount; ++i) {
    const idx = (i & 63) || 1;
    shouldBe(readThrough(holders[idx]), 7);
}
shouldBe(dictHolder.f.a, 99);
