//@ skip if not $jitTests
//@ runDefault("--useConcurrentJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

function hot(arr) {
    let sum = 0;
    for (const x of arr)
        (() => sum += x)();
    return sum;
}
noInline(hot);

const a = new Int32Array(1000).map((_, i) => i);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(hot(a), 499500);

// The per-iteration lexical scope for `const x` is populated by reading the
// previous iteration's value and writing it to the freshly-created scope. On
// the first iteration that value is the TDZ empty sentinel, which never makes
// it into the value profile, so FixupPhase used to insert an Int32 hint check
// at the PutClosureVar that BadType-exits once per call and never converges.
// We allow one recompile (the first attempt may still take the exit before the
// exit site is recorded), but not a recompile loop.
if (numberOfDFGCompiles(hot) > 2)
    throw new Error("hot() was recompiled " + numberOfDFGCompiles(hot) + " times; speculateForBarrier should back off after BadType exit");
