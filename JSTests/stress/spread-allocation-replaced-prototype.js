// Replacing a rest array's prototype makes its iteration protocol observable even though the realm's
// own Array.prototype is untouched, so the realm's iteration-protocol watchpoints cannot by
// themselves license Spread's fast path over that array.
//
// The array is still allocated by CreateRest in this realm, so any reasoning that reads the realm off
// the allocation opcode and stops there will conclude the protocol is primordial and either copy the
// butterfly directly or forward the arguments without iterating.
//
// Not skipped when the security fix is removed: the fast path is supposed to be declined here in
// every configuration.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

const customPrototype = {
    [Symbol.iterator]() {
        let done = false;
        return {
            next() {
                if (done)
                    return { done: true, value: undefined };
                done = true;
                return { done: false, value: 99 };
            }
        };
    }
};

function spreadWithReplacedPrototype(replace) {
    const rest = [1, 2, 3];
    if (replace)
        Object.setPrototypeOf(rest, customPrototype);
    return [...rest];
}
noInline(spreadWithReplacedPrototype);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(spreadWithReplacedPrototype(false).length, 3);

const spread = spreadWithReplacedPrototype(true);
shouldBe(spread.length, 1);
shouldBe(spread[0], 99);

// The same operand reached through varargs forwarding rather than an array literal, which is what
// ArgumentsEliminationPhase would try to eliminate outright. Eliminating the Spread here would skip
// the protocol entirely, so the phase has to decline as well.
function count(...args) {
    return args.length;
}

function forwardWithReplacedPrototype(replace, ...rest) {
    if (replace)
        rest.__proto__ = customPrototype;
    return count(...rest);
}
noInline(forwardWithReplacedPrototype);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(forwardWithReplacedPrototype(false, 1, 2, 3), 3);

shouldBe(forwardWithReplacedPrototype(true, 1, 2, 3), 1);
