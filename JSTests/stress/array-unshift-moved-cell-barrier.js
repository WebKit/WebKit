//@ requireOptions("--useDollarVM=1", "--numberOfGCMarkers=4", "--minimumGCPauseMS=0", "--gcPauseScale=0", "--maximumMutatorUtilization=0.01", "--useSamplingProfiler=true", "--sampleInterval=1")

// unshift on a Contiguous array moves the existing elements up to make room
// before publicLength is updated, so a moved cell is briefly outside the range
// a concurrent collector scans. Without a barrier on the array itself the
// collector can miss that cell and reclaim it while it is still reachable
// through the array. The inserted value is a double, so the store implies no
// barrier of its own.
//
// A cleared WeakRef whose only strong reference is the moved element means the
// collector lost track of a live cell.

function unshiftOne(array) {
    array.unshift(13.37);
}
noInline(unshiftOne);

// Monomorphic Contiguous receivers with spare capacity for ArrayUnshift, so
// the single-element fast path is what gets compiled.
const warmObject = { warmup: 0 };
for (let i = 0; i < testLoopCount; ++i) {
    const array = [warmObject, 0];
    array.pop();
    unshiftOne(array);
}

// Winning the race takes many attempts, but the configurations that lower
// testLoopCount are the ones where each attempt is most expensive, so scale
// both the victim pool and the round count with it. The pool is rounded down
// to a power of two so that the permutation below covers it exactly.
const victimCount = 1 << (28 - Math.clz32(testLoopCount));
const mask = victimCount - 1;
const victims = new Array(victimCount);
const weak = new Array(victimCount);

const rounds = Math.max(1, testLoopCount >> 9);

for (let round = 0; round < rounds; ++round) {
    for (let i = 0; i < victimCount; ++i) {
        // Reachable only through the victim's element, so nothing else keeps
        // it alive if the collector loses track of it.
        const cell = new Array(100).fill(i + 0.25);
        // Capacity for two elements while publicLength stays at one, which is
        // what the fast path requires.
        const victim = [cell, 0];
        victim.pop();
        victims[i] = victim;
        weak[i] = new WeakRef(cell);
    }

    // A deref'd WeakRef keeps its target alive until the next microtask
    // checkpoint, so drop that protection before the collection.
    releaseWeakRefs();
    $vm.gc(); // Pre-age the victims and clear newlyAllocated protection.
    $vm.gcSweepAsynchronously();

    // A full permutation decorrelates the mutator from the parallel markers'
    // LIFO worklists, and the 1us sampling interval supplies the frequent
    // preemption that exposes the window.
    for (let i = 0; i < victimCount; ++i)
        unshiftOne(victims[Math.imul(i, 0x9e3779b1) & mask]);

    // Let the collector finish this cycle.
    for (let i = 0; i < 2000; ++i)
        globalThis.safepointSink = { i };

    for (let i = 0; i < victimCount; ++i) {
        if (weak[i].deref() === undefined)
            throw new Error('collector reclaimed a cell still referenced by victims[' + i + ']');
    }
}
