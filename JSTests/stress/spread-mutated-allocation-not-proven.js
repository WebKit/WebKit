// Spread's fast path may only bypass Symbol.iterator when the operand's shape is established, not
// merely when the operand's realm is.
//
// A CreateRest or NewArrayBuffer operand pins the realm that allocated the array, so the realm's
// iteration-protocol watchpoints are the right ones to arm. It says nothing about what the array
// looks like by the time the spread runs: an own Symbol.iterator, a replaced prototype, or an
// indexing type the butterfly copy cannot walk are all still reachable. Treating the allocation
// opcode as a proof of shape lets the fast path copy the butterfly and ignore the operand's own
// iterator, and skips the runtime structure check that would have caught it.
//
// Not skipped when the security fix is removed: the fast path is supposed to be declined here in
// every configuration.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

function oneShotIterator() {
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

// Escaping the rest array through a call is what clobbers its abstract structure set, so the spread
// has no finite set to prove anything from - the case where an opcode-based shortcut would answer.
let escaped = null;
let installIterator = false;
function escape(array) {
    escaped = array;
    if (installIterator)
        array[Symbol.iterator] = oneShotIterator;
}
noInline(escape);

function spreadEscapedRest(...rest) {
    escape(rest);
    return [...rest];
}
noInline(spreadEscapedRest);

for (let i = 0; i < testLoopCount; ++i) {
    const result = spreadEscapedRest(1, 2, 3);
    shouldBe(result.length, 3);
    shouldBe(result[0], 1);
}

// The iterator yields exactly one value, so anything of length 3 means the butterfly was copied and
// the operand's own Symbol.iterator was never consulted.
installIterator = true;
const withOwnIterator = spreadEscapedRest(1, 2, 3);
shouldBe(withOwnIterator.length, 1);
shouldBe(withOwnIterator[0], 99);
installIterator = false;

// A named property transitions the rest array to a structure that is not one of the realm's original
// array structures, while leaving the abstract structure set finite and in the same realm. Spreading
// it is legal and observes the three elements; the transition must not be mistaken for a violated
// invariant.
function spreadRestWithProperty(...rest) {
    rest.foo = 1;
    return [...rest];
}
noInline(spreadRestWithProperty);

for (let i = 0; i < testLoopCount; ++i) {
    const result = spreadRestWithProperty(1, 2, 3);
    shouldBe(result.length, 3);
    shouldBe(result[2], 3);
}

// Same shape for an array literal, whose operand is NewArrayBuffer rather than CreateRest.
function spreadLiteralWithOwnIterator(install) {
    const a = [1, 2, 3];
    if (install)
        a[Symbol.iterator] = oneShotIterator;
    return [...a];
}
noInline(spreadLiteralWithOwnIterator);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(spreadLiteralWithOwnIterator(false).length, 3);

const literalWithOwnIterator = spreadLiteralWithOwnIterator(true);
shouldBe(literalWithOwnIterator.length, 1);
shouldBe(literalWithOwnIterator[0], 99);
