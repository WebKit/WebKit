//@ runDefault("--weakBlockPoolDivisor=0", "--useDollarVM=1", "--collectContinuously=1", "--verifyGC=1")
//@ runDefault("--weakBlockPoolDivisor=1", "--useDollarVM=1", "--collectContinuously=1", "--verifyGC=1")
//@ runDefault("--weakBlockPoolDivisor=16", "--useDollarVM=1", "--stealEmptyBlocksFromOtherAllocators=1", "--useGenerationalGC=0", "--verifyGC=1")

// Weak handles are returned to their WeakBlock as soon as they are cleared, and a WeakBlock goes
// back to the Heap as soon as it is free, so a WeakSet holds no blocks once its MarkedBlock is
// empty. That is what lets an empty MarkedBlock be stolen by another BlockDirectory. Exercise the
// three ways a block leaves a WeakSet: freed outright, detached while finalized handles remain,
// and released when the container itself goes away.

const count = testLoopCount;

function churnRegExps() {
    // RegExpCache holds each compiled RegExp in a Weak, so this allocates and drops WeakImpls
    // against the RegExp subspace.
    let live = [];
    for (let i = 0; i < count; ++i)
        live.push(new RegExp(`x{${i % 9}}y${i}`, "g"));
    if (live.length !== count)
        throw new Error(`expected ${count} RegExps, got ${live.length}`);
    return live[count - 1].source;
}

function churnWeakRefs() {
    // A WeakRef whose target dies leaves a finalized handle behind for as long as the WeakRef
    // itself is alive, which is what produces a logically empty block detached to the Heap.
    let refs = [];
    for (let i = 0; i < count; ++i)
        refs.push(new WeakRef({ i }));
    $vm.gc();
    let survivors = 0;
    for (const ref of refs) {
        if (ref.deref() !== undefined)
            ++survivors;
    }
    // Dropping the WeakRefs is what finally frees those detached blocks.
    refs = null;
    return survivors;
}

function churnMaps() {
    let live = [];
    for (let i = 0; i < count; ++i)
        live.push(new Map([[i, i + 1]]));
    if (live.length !== count)
        throw new Error(`expected ${count} Maps, got ${live.length}`);
    return live[count - 1].get(count - 1);
}

const startBlocks = $vm.weakBlockCount();

for (let round = 0; round < 4; ++round) {
    if (churnRegExps() === undefined)
        throw new Error(`round ${round}: lost a RegExp`);
    churnWeakRefs();
    if (churnMaps() !== count)
        throw new Error(`round ${round}: lost a Map`);
    $vm.gc();
}

$vm.gc();
$vm.gc();

// Every handle above is unreachable now, so the blocks that held them must have gone back to the
// Heap rather than staying pinned to the WeakSets of the MarkedBlocks they belonged to.
const leaked = $vm.weakBlockCount() - startBlocks;
if (leaked > 8)
    throw new Error(`${leaked} WeakBlocks outlived every handle in them`);
