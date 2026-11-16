// A varargs call whose argument count grows past the profiled maximum makes the LoadVarargs
// speculation fail. That exit is recorded against the makeCall checkpoint of the call bytecode
// rather than the instruction start, so the parser has to consult every checkpoint of the index
// before inlining the call again. Otherwise it re-inlines with the same too-small limit and exits
// again, relearning the same lesson on every recompile.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

function callee() {
    let total = 0;
    for (let i = 0; i < arguments.length; ++i)
        total += arguments[i];
    return total;
}

function caller(args) {
    return callee.apply(null, args);
}
noInline(caller);

// Grow the argument count in stages. Each stage runs long enough to be optimized with a limit that
// the next stage then exceeds.
let args = [1, 2, 3];
for (let stage = 0; stage < 6; ++stage) {
    let expected = 0;
    for (const value of args)
        expected += value;

    for (let i = 0; i < 20000; ++i)
        shouldBe(caller(args), expected);

    const grown = args.slice();
    for (let i = 0; i < 8; ++i)
        grown.push(stage * 8 + i);
    args = grown;
}

// Alternating between a short and a long argument list must still produce correct results.
const short = [1, 2, 3];
let longExpected = 0;
for (const value of args)
    longExpected += value;
for (let i = 0; i < 20000; ++i) {
    shouldBe(caller(short), 6);
    shouldBe(caller(args), longExpected);
}
