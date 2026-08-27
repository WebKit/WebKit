//@ skip if $secAccel3Removed
//@ requireOptions("--useConcurrentJIT=false", "--useConcurrentGC=false", "--sweepSynchronously=true")

function cmp(a, b) { return 0; }
noInline(cmp);

// Overwrites anything left on the stack below this frame.
function consumeStack(depth) {
    let a = depth ^ 1, b = depth ^ 2, c = depth ^ 3, d = depth ^ 4, e = depth ^ 5;
    let f = depth ^ 6, g = depth ^ 7, h = depth ^ 8, i = depth ^ 9, j = depth ^ 10;
    if (depth > 0)
        return consumeStack(depth - 1) + a + b + c + d + e + f + g + h + i + j;
    return 0;
}
noInline(consumeStack);

function makePlain() {
    return [{a:1},{a:2},{a:3},{a:4}];
}
noInline(makePlain);

function makePlainWithHole() {
    return [{a:1}, undefined, {a:2}];
}
noInline(makePlainWithHole);

// Date instances get their own MarkedBlocks (dateInstanceSpace), so a batch of them that dies
// together leaves blocks with no live cells at all. The undefined sits in the middle so elements
// are copied before it is reached whichever direction the copy loop runs.
function makeDatesWithHole() {
    const batch = [];
    for (let i = 0; i < 96; ++i)
        batch.push(new Date(i));
    return [batch[0],batch[1],batch[2],batch[3],batch[4],batch[5],batch[6],
            undefined,
            batch[7],batch[8],batch[9],batch[10],batch[11],batch[12]];
}
noInline(makeDatesWithHole);

function clear(a) {
    for (let i = 0; i < a.length; ++i)
        a[i] = 0;
    a.length = 0;
}
noInline(clear);

// Separate frame: keeps this call's result from overwriting the caller's live values.
function sortWithHole(a) {
    a.sort(cmp);
}
noInline(sortWithHole);

function driver(plain, full) {
    plain.sort(cmp);
    if (!full)
        return plain;

    edenGC();
    let dates = makeDatesWithHole();
    sortWithHole(dates);
    clear(dates);
    dates = null;
    consumeStack(400);
    edenGC();
    fullGC();
    return plain;
}
noInline(driver);

// Tier up sortWithHole
for (let i = 0; i < testLoopCount; ++i)
    sortWithHole(makePlainWithHole());

// Tier up driver, but usually skip the expensive parts to keep the test fast.
let fullEvery = Math.max(1, (testLoopCount / 20) | 0);
for (let i = 0; i < testLoopCount; ++i)
    driver(makePlain(), (i % fullEvery) === 0);

// Try to trigger the bug
for (let i = 0; i < 20; ++i)
    driver(makePlain(), true);
