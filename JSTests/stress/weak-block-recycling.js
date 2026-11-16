//@ runDefault("--weakBlockPoolDivisor=0", "--useDollarVM=1", "--collectContinuously=1", "--verifyGC=1")
//@ runDefault("--weakBlockPoolDivisor=1", "--useDollarVM=1", "--collectContinuously=1", "--verifyGC=1")
//@ runDefault("--weakBlockPoolDivisor=16", "--useDollarVM=1", "--stealEmptyBlocksFromOtherAllocators=1", "--useGenerationalGC=0", "--verifyGC=1")

// A weak handle lives in a WeakBlock belonging to the WeakSet of its target's MarkedBlock, and a
// WeakBlock goes back to the Heap as soon as its last handle is gone, so a WeakSet holds no blocks
// once its MarkedBlock is empty. That is what lets an empty MarkedBlock be stolen by another
// BlockDirectory. Exercise the two ways a block leaves a WeakSet, emptied in place and detached
// while finalized handles remain, and then steal the MarkedBlocks they belonged to.

const count = testLoopCount;

// $vm.Element registers itself with its $vm.Root through a weak handle, and the two are destroyed
// in separate sweeps: the handle is finalized when the Element's block is swept, but it is only
// dropped once the Root is swept in turn. In between, its block holds nothing but finalized handles
// and is detached to the Heap.
function churnOpaqueRoots() {
    for (let i = 0; i < count; ++i)
        new $vm.Element(new $vm.Root());
}

// RegExpCache holds each compiled RegExp in a Weak whose finalizer drops the handle immediately, so
// these blocks empty out inside the WeakSet that owns them.
function churnRegExps() {
    let live = [];
    for (let i = 0; i < count; ++i)
        live.push(new RegExp(`x{${i % 9}}y${i}`, "g"));
    if (live.length !== count)
        throw new Error(`expected ${count} RegExps, got ${live.length}`);
    return live[count - 1].source;
}

// Every ArrayBuffer keeps a weak handle to its JS wrapper, against a different subspace again.
function churnArrayBuffers() {
    let live = [];
    for (let i = 0; i < count; ++i)
        live.push(new ArrayBuffer(8));
    return live.length;
}

// Spread over enough size classes that the directories left short of blocks steal the empty ones
// the churn above just abandoned.
function churnSizeClasses() {
    let live = [];
    for (let i = 0; i < count; ++i) {
        let object = {};
        for (let j = 0; j < i % 24; ++j)
            object["p" + j] = j;
        live.push(object, new Array(i % 48));
    }
    return live.length;
}

const startBlocks = $vm.weakBlockCount();
let peakBlocks = startBlocks;

for (let round = 0; round < 4; ++round) {
    churnOpaqueRoots();
    if (churnRegExps() === undefined)
        throw new Error(`round ${round}: lost a RegExp`);
    if (churnArrayBuffers() !== count)
        throw new Error(`round ${round}: lost an ArrayBuffer`);
    peakBlocks = Math.max(peakBlocks, $vm.weakBlockCount());
    // An eden collection leaves emptied blocks pooled in the Heap for the next round to allocate
    // out of, where a full one hands them all back.
    $vm.edenGC();
    if (churnSizeClasses() !== 2 * count)
        throw new Error(`round ${round}: lost an object`);
    $vm.edenGC();
}

// A run that allocated no WeakBlock at all would sail through the check below.
if (peakBlocks <= startBlocks)
    throw new Error(`no WeakBlock was ever allocated: stayed at ${startBlocks}`);

$vm.gc();
$vm.gc();

// Every handle above is unreachable now, so the blocks that held them must have gone back to the
// Heap rather than staying pinned to the WeakSets of the MarkedBlocks they belonged to.
const leaked = $vm.weakBlockCount() - startBlocks;
if (leaked > 8)
    throw new Error(`${leaked} WeakBlocks outlived every handle in them`);
