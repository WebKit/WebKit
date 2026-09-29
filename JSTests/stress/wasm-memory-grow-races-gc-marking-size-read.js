//@ skip if $secAccel3Removed
//@ slow!
//@ runDefault("--collectContinuously=1")

// Regression test for the GC-marking-thread half of rdar://186122719.
//
// This is a probabilistic test that pays off over many ASan runs. Raise ROUNDS to make one run
// more likely to catch it.

const MEMORIES = 256;   // live memories, all of them visited by every marking cycle
const MAX_PAGES = 8;    // grows per memory before it is replaced; small keeps grow() cheap
const ROUNDS = 80;

const mems = [];
for (let i = 0; i < MEMORIES; ++i)
    mems.push(new WebAssembly.Memory({ initial: 1, maximum: MAX_PAGES }));

for (let round = 0; round < ROUNDS; ++round) {
    for (let i = 0; i < MEMORIES; ++i) {
        try {
            mems[i].grow(1);
        } catch (e) {
            // Exhausted this memory's maximum. Replace it, so marking keeps finding a memory whose
            // handle is still being replaced, and so the old one becomes garbage for the collector.
            mems[i] = new WebAssembly.Memory({ initial: 1, maximum: MAX_PAGES });
        }
    }
}
