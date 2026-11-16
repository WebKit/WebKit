//@ runDefault
//@ requireOptions("--useConcurrentJIT=0")
//
// Verifies that triggering OSR exit inside the comparator (mid-sort, after the
// DFG ArraySortIntrinsic's inlined body has started rearranging the receiver)
// still produces a correctly sorted array with the original multiset intact.
//
// The in-place insertion sort in the DFG intrinsic mutates the receiver. When
// OSR exit fires from inside the comparator, baseline re-runs `op_call`,
// which calls the generic C++ sort on the partially-sorted receiver. Because
// sort is idempotent on any intermediate state (and takes a snapshot of the
// element contents), the receiver must end up both:
//   (1) correctly sorted according to the comparator's ordering, and
//   (2) with its original multiset preserved: no duplicates, no missing,
//       no corrupted values.

let exitCount = 0;
let callCount = 0;

function inlinedComparator(a, b) {
    callCount++;
    if ($vm.ftlTrue()) {
        exitCount++;
        OSRExit();
    }
    return a - b;
}

function test(arr) {
    arr.sort(inlinedComparator);
}

const original = [5, 3, 8, 1, 9, 4, 7, 2, 6, 0, 11, 14, 13, 10, 12, 15];
const expectedSorted = original.slice().sort((a, b) => a - b);

function multiset(a) {
    const counts = new Map();
    for (const v of a)
        counts.set(v, (counts.get(v) || 0) + 1);
    return counts;
}

function sameMultiset(a, b) {
    const ma = multiset(a);
    const mb = multiset(b);
    if (ma.size !== mb.size) return false;
    for (const [k, v] of ma)
        if (mb.get(k) !== v) return false;
    return true;
}

// Warm up so DFG and then FTL compile `test`; once FTL is hot, every sort
// call will trigger at least one OSRExit() inside the comparator.
for (let i = 0; i < testLoopCount; i++) {
    const copy = original.slice();
    test(copy);

    // Final state must have the original multiset of elements.
    if (!sameMultiset(copy, original))
        throw new Error("iter " + i + ": receiver's multiset changed:\n"
            + "  original=" + JSON.stringify(original) + "\n"
            + "  result  =" + JSON.stringify(copy));

    // Final state must be sorted.
    for (let j = 0; j < expectedSorted.length; j++) {
        if (copy[j] !== expectedSorted[j])
            throw new Error("iter " + i + " index " + j
                + ": got " + copy[j] + " expected " + expectedSorted[j]
                + " (exitCount=" + exitCount + ")");
    }
}

if (exitCount === 0 && $vm.useFTLJIT())
    throw new Error("expected at least one OSR exit during the run");
