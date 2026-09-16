//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// for-in writes to a claimed field: op_enumerator_put_by_val's OwnStructureMode fast path stores at a cached
// offset in all four tiers, bypassing every field-type hook, so JSPropertyNameEnumerator::create() permanently
// withdraws the claim on every own-structure offset -- there is no claim left to violate. The enumerated object
// and the written object must be the SAME object, or the guard (structure->id() == cachedStructureID()) fails and
// the write degrades to the hooked generic path.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Strength() { this.value = 111; }
function Other() { this.a = 1; this.b = 2; this.c = 3; }   // different structure, different layout

function Holder(p) { this.f = p; }

const holders = [];
for (let i = 0; i < 200; ++i)
    holders.push(new Holder(new Strength()));

function read(h) { return h.f.value; }
noInline(read);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(read(holders[i % 200]), 111);
gc();

// This ordinary same-shape store is load-bearing: reading h.f installed a GetById IC, which sets
// isWatchingReplacement, and the OwnStructureMode guard rejects that, so without it the for-in write degrades to
// the hooked generic path. A replace store clears the bit while leaving the claim live -- the dangerous state.
const victim = holders[0];
victim.f = new Strength();

// Write through for-in, to the same object being enumerated, with a value that contradicts the claim. A surviving
// claim would let `read` load Strength's `value` offset, which in an Other is `b`.
for (const k in victim)
    victim[k] = new Other();

shouldBe(victim.f.b, 2);
shouldBe(read(victim), undefined);
gc();

for (let round = 0; round < 4; ++round) {
    // Distinct source per round, but no arithmetic on the loaded value: `undefined + 0 - 0` is NaN.
    const fresh = new Function("h", "if (h === null) return " + round + "; return h.f.value;");
    noInline(fresh);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(fresh(victim), undefined);
    gc();
}

for (let i = 1; i < holders.length; ++i)
    shouldBe(holders[i].f.value, 111);

// Repeat while the loop is hot, so the baseline and DFG/FTL fast paths do the storing.
function forInStore(o, v) {
    for (const k in o)
        o[k] = v;
}
noInline(forInStore);

const hammered = [];
for (let i = 0; i < 64; ++i)
    hammered.push(new Holder(new Strength()));
for (let i = 0; i < testLoopCount; ++i)
    forInStore(hammered[i & 63], new Strength());   // same structure: claim stays satisfiable
// Sweep all 64 explicitly: testLoopCount is small in mini-mode and the eager-JIT configurations, so `i & 63`
// over testLoopCount iterations does not necessarily touch every element.
for (let i = 0; i < 64; ++i)
    forInStore(hammered[i], new Strength());
gc();
for (let i = 0; i < 64; ++i)
    shouldBe(hammered[i].f.value, 111);

for (let i = 0; i < testLoopCount; ++i)
    forInStore(hammered[i & 63], new Other());
for (let i = 0; i < 64; ++i)
    forInStore(hammered[i], new Other());
gc();
for (let i = 0; i < 64; ++i)
    shouldBe(hammered[i].f.b, 2);
