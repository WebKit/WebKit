//@ runDefault("--stealEmptyBlocksFromOtherAllocators=1", "--useGenerationalGC=0", "--collectContinuously=1", "--verifyGC=1")

// Fills one subspace, drops it, then fills a different one, so that the second round has to take
// the MarkedBlocks the first round left behind. The two rounds pick cell types that land in
// different BlockDirectories on the same AlignedMemoryAllocator: plain objects with a fixed shape,
// closures, and butterflies. collectContinuously runs the sweeper against the allocator's list of
// directories with empty blocks while all of this is happening.

function makeObjects(count) {
    let result = [];
    for (let i = 0; i < count; ++i)
        result.push({ a: i, b: i + 1, c: i + 2 });
    return result;
}

function makeClosures(count) {
    let result = [];
    for (let i = 0; i < count; ++i)
        result.push(() => i);
    return result;
}

function makeArrays(count) {
    let result = [];
    for (let i = 0; i < count; ++i)
        result.push(new Array(16).fill(i));
    return result;
}

function makeDestructibleCells(count) {
    // Maps and RegExps have destructors, so their blocks owe a destructor pass before anyone else
    // can use them. Handing one of those over has to run the old owner's destructors first.
    let result = [];
    for (let i = 0; i < count; ++i) {
        result.push(new Map([[i, i + 1]]));
        result.push(new RegExp(`a{${i % 7}}b`, "g"));
    }
    return result;
}

const rounds = [makeObjects, makeClosures, makeArrays, makeDestructibleCells];
const count = testLoopCount >> 3;

for (let round = 0; round < 2 * rounds.length; ++round) {
    let make = rounds[round % rounds.length];
    let live = make(count);
    let expected = make === makeDestructibleCells ? 2 * count : count;
    if (live.length !== expected)
        throw new Error(`round ${round} allocated ${live.length} cells, expected ${expected}`);

    // Touch the tail so nothing above is optimized away, then drop the whole batch. The next round
    // allocates a different cell type and has to reuse these blocks across subspaces.
    if (live[expected - 1] === undefined)
        throw new Error(`round ${round} lost its last cell`);
    live = null;

    gc();
}
