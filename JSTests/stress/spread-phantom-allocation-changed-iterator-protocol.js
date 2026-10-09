// Not escaping a CreateRest or NewArrayBuffer lets ArgumentsEliminationPhase convert it to a
// PhantomCreateRest or PhantomNewArrayBuffer, and the FTL lowers a Spread over either of those by
// copying a butterfly straight out of the phantom allocation, never consulting Symbol.iterator. So
// declining to escape the allocation is itself a promise that the realm's iteration protocol is
// primordial, and it has to arm the watchpoints that keep it that way.
//
// The Spread here is not itself an elimination candidate - it feeds a NewArrayWithSpread, which is
// not one either - so the arming that guards the candidate path is never reached. ArrayUse on the
// Spread does not cover this: it is a speculation about the operand's shape, backed by a runtime
// check, and says nothing about whether Array.prototype[Symbol.iterator] is still primordial.
//
// This only reproduces in the FTL, since ArgumentsEliminationPhase runs on SSA.
//
// Not skipped when the security fix is removed: the iterator is supposed to be called in every
// configuration.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

// Replaced before any warmup, so the realm's watchpoint is already invalid by the time anything is
// compiled. It yields exactly one value, so a result of any other length means the butterfly was
// copied instead.
Array.prototype[Symbol.iterator] = function () {
    let done = false;
    return {
        next() {
            if (done)
                return { done: true, value: undefined };
            done = true;
            return { done: false, value: 42 };
        }
    };
};

function spreadRest(...rest) {
    return [...rest];
}
noInline(spreadRest);

for (let i = 0; i < testLoopCount; ++i) {
    const result = spreadRest(1, 2, 3);
    shouldBe(result.length, 1);
    shouldBe(result[0], 42);
}

function spreadLiteral() {
    return [...[1, 2, 3]];
}
noInline(spreadLiteral);

for (let i = 0; i < testLoopCount; ++i) {
    const result = spreadLiteral();
    shouldBe(result.length, 1);
    shouldBe(result[0], 42);
}
