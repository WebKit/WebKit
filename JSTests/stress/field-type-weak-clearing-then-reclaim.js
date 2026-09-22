//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Weak clearing must withdraw the shape-carried copy of the claim, not only the table record. When the claimed
// structure dies pruneAfterMarking clears the record, and the Structure's cached word has to be cleared in the
// same breath: the invariant is that the word never claims more than the table does. A leftover word is not a
// use-after-free -- the fast path compares raw bits and never decodes them -- but it silently denies the field any
// chance of being claimed again, and if a recycled ID happened to match, an object of a different structure would
// be waved through.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function readF(h) { return h.f; }
function readTag(h) { return h.f.tag; }
noInline(readF);
noInline(readTag);

function Holder(v) { this.f = v; }

// The pointee constructor is built per round, so each round's claimed structure dies with the round.
function runRound(round) {
    const Pointee = new Function("this.tag = " + round + "; this.r" + round + " = 1;");
    const hs = [];
    for (let i = 0; i < 32; ++i)
        hs.push(new Holder(new Pointee()));
    for (let i = 0; i < 2000; ++i)
        shouldBe(readTag(hs[i & 31]), round);
    // The claimed structure becomes garbage while the record still names it.
    hs.length = 0;
}

for (let round = 0; round < 16; ++round) {
    runRound(round);
    gc();   // the collection that proves the claimed structure dead and clears the claim
}

// The field must still be re-claimable: a word left naming the dead ID would make the fast path skip these.
function Fresh(tag) { this.tag = tag; this.extra = tag * 2; }
const live = [];
for (let i = 0; i < 64; ++i)
    live.push(new Holder(new Fresh(i)));
gc();
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readTag(live[i & 63]), i & 63);

function Different(tag) { this.other = tag; this.tag = tag + 100; }
for (let i = 0; i < 32; ++i)
    live[i].f = new Different(i);
gc();
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    shouldBe(readTag(live[idx]), idx < 32 ? idx + 100 : idx);
}

// Non-cell through the same field, then cells again, after weak clearing has run many times.
for (let i = 0; i < 64; ++i)
    live[i].f = i;
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readF(live[i & 63]), i & 63);
for (let i = 0; i < 64; ++i)
    live[i].f = new Fresh(i);
gc();
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readTag(live[i & 63]), i & 63);
