// Stress the DFG iterator_open / iterator_next fast path when a single
// call site observes Array, Map, Set, and a Generic user-defined iterable.
// The seenModes IterationMetadata accumulates FastArray|FastMap|FastSet|Generic,
// and the DFG parser emits the full chained fast path. This exercises the
// connectFailedBlock/numberOfRemainingModes bookkeeping across all four modes.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad: ' + actual + ' vs ' + expected);
}
noInline(shouldBe);

function sumIterable(iterable) {
    let sum = 0;
    for (const entry of iterable) {
        if (typeof entry === "number")
            sum += entry;                   // Array / Set yields values
        else if (Array.isArray(entry))
            sum += entry[0] + entry[1];     // Map yields [key, value]
        else
            sum += entry.val;               // Generic iterator yields {val}
    }
    return sum;
}
noInline(sumIterable);

const arr = [1, 2, 3, 4];
const map = new Map([[10, 1], [20, 2], [30, 3]]);
const set = new Set([100, 200, 300]);

function makeGeneric() {
    let i = 0;
    return {
        [Symbol.iterator]() { return this; },
        next() {
            if (i >= 4)
                return { value: undefined, done: true };
            return { value: { val: ++i * 1000 }, done: false };
        }
    };
}

const arrSum = 1 + 2 + 3 + 4;
const mapSum = (10 + 1) + (20 + 2) + (30 + 3);
const setSum = 100 + 200 + 300;
const genSum = 1000 + 2000 + 3000 + 4000;

for (let i = 0; i < testLoopCount; i++) {
    const idx = i & 3;
    if (idx === 0)
        shouldBe(sumIterable(arr), arrSum);
    else if (idx === 1)
        shouldBe(sumIterable(map), mapSum);
    else if (idx === 2)
        shouldBe(sumIterable(set), setSum);
    else
        shouldBe(sumIterable(makeGeneric()), genSum);
}
