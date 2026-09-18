//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1", "--useDollarVM=1")

// Ancestor claim withdrawal when the recycled offset is the shape's maximum offset.

// Structure::add sets maxOffset = max(newOffset, oldMaxOffset), so a fresh offset and a recycled maximum offset
// look identical from the owner alone; the withdrawal guard must also consult previous->maxOffset().

// Asserts fieldTypeAncestorWithdrawalCount() > 0 on the TOTAL, not a delta around one loop: earlier loops here
// already withdraw, so a delta passes even with the mechanism off. Value checks alone were inert against it.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Strength() { this.value = 111; }
function Other() { this.a = 1; this.b = 2; this.c = 3; }

// `claimed` is added LAST, so it holds the shape's maximum offset.
function make(v) {
    const o = {};
    o.x = 1;
    o.y = 2;
    o.claimed = v;      // highest offset on this shape
    return o;
}

const kept = [];
for (let i = 0; i < 200; ++i)
    kept.push(make(new Strength()));

function read(o) { return o.claimed.value; }
noInline(read);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(read(kept[i % 200]), 111);
gc();

// Delete the maximum-offset property and add a different one, which recycles that exact offset.
const victims = [];
for (let i = 0; i < 200; ++i) {
    const o = make(new Strength());
    delete o.claimed;
    o.reused = new Other();   // same offset the deleted `claimed` occupied
    victims.push(o);
}
gc();

for (let i = 0; i < 200; ++i) {
    shouldBe(victims[i].reused.b, 2);
    shouldBe(victims[i].claimed, undefined);
    shouldBe(victims[i].x, 1);
    shouldBe(victims[i].y, 2);
}

// Fresh compilations must not adopt a stale claim on the recycled offset.
for (let round = 0; round < 4; ++round) {
    const freshReused = new Function("o", "if (o === null) return " + round + "; return o.reused.b;");
    noInline(freshReused);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(freshReused(victims[i % 200]), 2);
    gc();
}

for (let i = 0; i < 200; ++i)
    shouldBe(kept[i].claimed.value, 111);

// Reverse order: recycle the offset with a satisfying value, then contradict it, so the withdrawal happens on the
// store rather than at creation.
const second = [];
for (let i = 0; i < 200; ++i) {
    const o = make(new Strength());
    delete o.claimed;
    o.reused = new Strength();
    second.push(o);
}
gc();
for (let i = 0; i < 200; ++i)
    second[i].reused = new Other();
gc();
for (let i = 0; i < 200; ++i)
    shouldBe(second[i].reused.b, 2);


// The case the guard exists for: the last-added, claimed property is deleted and replaced at the same offset by a
// property holding a different structure.
const forced = [];
for (let i = 0; i < 64; ++i) {
    const o = {};
    o.x = 1;
    o.y = 2;
    o.claimed = new Strength();   // maximum offset on this shape, and claimed
    delete o.claimed;
    o.reused = new Other();       // same offset, contradicting structure
    forced.push(o);
}
gc();
for (let i = 0; i < 64; ++i)
    shouldBe(forced[i].reused.b, 2);

const withdrawals = $vm.fieldTypeAncestorWithdrawalCount();
if (!(withdrawals > 0)) {
    throw new Error("ancestor withdrawal never happened: fieldTypeAncestorWithdrawalCount is 0. A claim keyed on "
        + "an ancestor at a reused offset was left standing, which is the defect this test exists for. The "
        + "assertion is on the withdrawal itself rather than on values, because when the mechanism was still "
        + "disabling that option killed no test in the suite, because the only test reaching this path "
        + "asserted values rather than the withdrawal itself.");
}
