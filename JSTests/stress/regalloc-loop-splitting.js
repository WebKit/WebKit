//@ requireOptions("--airGreedyRegAllocSplitAroundLoops=true")

// Exercises loop-aware live range splitting in the greedy register allocator.
// Many variables live across a loop with a clobber call creating register pressure.

function clobber(x) { return x | 0; }
noInline(clobber);

function test(p) {
    let v0  = p + 0;
    let v1  = p + 1;
    let v2  = p + 2;
    let v3  = p + 3;
    let v4  = p + 4;
    let v5  = p + 5;
    let v6  = p + 6;
    let v7  = p + 7;
    let v8  = p + 8;
    let v9  = p + 9;
    let v10 = p + 10;
    let v11 = p + 11;
    let v12 = p + 12;
    let v13 = p + 13;
    let v14 = p + 14;
    let v15 = p + 15;

    let sum = 0;
    for (let i = 0; i < 100; i++)
        sum = clobber(sum + i);

    return v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 +
           v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + sum;
}
noInline(test);

let expected = 5086; // v-sum = 136, loop sum = 4950
for (let i = 0; i < testLoopCount; i++) {
    let result = test(1);
    if (result !== expected)
        throw "FAIL: expected " + expected + " but got " + result;
}
