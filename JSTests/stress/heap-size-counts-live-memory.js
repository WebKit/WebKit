//@ runDefault

// A collection counts the memory of the cells it finds live: their bytes, as the old generation's size, and the memory
// outside the heap they report as extra memory. An Eden collection adds the young cells it finds live to both, and
// older cells stay counted without being revisited.

function shouldBeAtLeast(actual, expected, description)
{
    if (actual < expected)
        throw new Error(`${description}: ${actual} is less than ${expected}`);
}

const arraysPerBatch = 4;
const arrayBytes = 1 << 20; // Above JSArrayBufferView::fastSizeLimit, so stored outside the heap and reported as extra memory.
const objectsPerBatch = 50000;
const minimumObjectBytes = 32; // A cell header, a butterfly pointer, and some of the four property slots.
const extraMemoryPerBatch = arraysPerBatch * arrayBytes;
const cellBytesPerBatch = objectsPerBatch * minimumObjectBytes;

// Only this array references the batches, and they're allocated in a separate frame, which keeps stale references off
// the stack where the conservative scan would find them.
let live = [];
function allocateBatch()
{
    for (let i = 0; i < arraysPerBatch; ++i)
        live.push(new Uint8Array(arrayBytes));
    for (let i = 0; i < objectsPerBatch; ++i)
        live.push({ a: i, b: i, c: i, d: i });
}
noInline(allocateBatch);

function measure(collectFn)
{
    let heapSize = collectFn();
    let extraMemory = $vm.heapExtraMemorySize();
    return { extraMemory, oldGenerationSize: heapSize - extraMemory };
}

function shouldHaveGrownBy(after, before, batches, description)
{
    shouldBeAtLeast(after.extraMemory - before.extraMemory, batches * extraMemoryPerBatch, `extra memory ${description}`);
    shouldBeAtLeast(after.oldGenerationSize - before.oldGenerationSize, batches * cellBytesPerBatch, `old generation ${description}`);
}

let baseline = measure(fullGC);

allocateBatch();
let afterFirstEden = measure(edenGC);
shouldHaveGrownBy(afterFirstEden, baseline, 1, "after an Eden collection");

// The second Eden collection doesn't revisit the first batch, which has to stay counted.
allocateBatch();
let afterSecondEden = measure(edenGC);
shouldHaveGrownBy(afterSecondEden, baseline, 2, "after a second Eden collection");

let afterFull = measure(fullGC);
shouldHaveGrownBy(afterFull, baseline, 2, "after a Full collection");

// Release all batches: shrinking the array clears its slots, so nothing references the batches any more.
live.length = 0;

// An Eden collection doesn't revisit old cells, so it can't find the batches dead, and they stay counted.
let afterReleaseEden = measure(edenGC);
shouldHaveGrownBy(afterReleaseEden, baseline, 2, "after an Eden collection with the batches dead");

let afterRelease = measure(fullGC);
let extraMemoryReclaimed = afterFull.extraMemory - afterRelease.extraMemory;
let oldGenerationReclaimed = afterFull.oldGenerationSize - afterRelease.oldGenerationSize;
// Allow for a few cells still found by the conservative stack scan.
shouldBeAtLeast(extraMemoryReclaimed, extraMemoryPerBatch, "extra memory reclaimed once the batches are dead");
shouldBeAtLeast(oldGenerationReclaimed, cellBytesPerBatch, "old generation reclaimed once the batches are dead");
