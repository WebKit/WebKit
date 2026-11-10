//@ runDefault("--collectContinuously=true", "--useGenerationalGC=false", "--weakBlockPoolDivisor=0")

// A WeakHolder keeps its value in a Weak<> and visits it, so concurrent markers read that Weak<>
// while the mutator replaces it. Freeing an emptied WeakBlock then would free memory a marker may
// still be reading.

// At 1 this reproduces in roughly 1 in 6 runs; 10 reproduces nearly every run.
const scale = 1;

// A live heap keeps each collection in concurrent marking for longer.
let ballast = [];
for (let i = 0; i < 1e4 * scale; ++i)
    ballast.push({ i });

// Each batch's targets are allocated together, so their Weak<>s fill WeakBlocks no other handle shares.
const batchSize = 1000;
for (let round = 0; round < 5 * scale; ++round) {
    let holders = [];
    for (let i = 0; i < batchSize; ++i)
        holders.push(createWeakHolder({ i }));
    for (let i = 0; i < batchSize; ++i)
        setWeakHolderValue(holders[i], i);
}
