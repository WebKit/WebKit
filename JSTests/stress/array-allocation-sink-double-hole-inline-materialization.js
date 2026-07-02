
function test(i) {
    let arr = new Array(4);
    arr[0] = 1.1;
    arr[2] = 3.3;
    if (i & 1) arr[1] = 2.2;
    arr[0] = i + 0.5;
    // Escape on ~50% of iterations so the FTL compiles this as an inline
    // materialization (not an OSR exit).
    if (i & 2) return arr;
}
noInline(test);

for (let i = 0; i < testLoopCount; i++) {
    let arr = test(i);
    if (arr) {
        // i & 2 is true. Check hole vs written depending on i & 1.
        if (i & 1) {
            // arr[1] was written
            if (arr[1] !== 2.2)
                throw new Error("expected 2.2 at i=" + i + ", got " + arr[1]);
        } else {
            // arr[1] is a hole
            if (arr[1] !== undefined)
                throw new Error("expected undefined at i=" + i + ", got " + arr[1]);
            if (1 in arr)
                throw new Error("expected hole at i=" + i);
        }
    }
}
