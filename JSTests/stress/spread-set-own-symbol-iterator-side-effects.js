//@ runDefault("--useConcurrentJIT=0")

// Spread(SetObjectUse): the abstract interpreter must not retain structure
// proofs across the node when the operand isn't proven to carry the original
// Set structure, since the lowered slow path (operationSpreadSet) can invoke a
// user-defined Symbol.iterator.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value");
}

let obj;

// Pre-create the {a}->Accessor transition so the warmup structure isn't a watchable leaf.
{
    let t = { a: 1.1 };
    Object.defineProperty(t, "a", { get() { return 1; } });
}

let setWithOwnIterator = new Set([1]);
setWithOwnIterator[Symbol.iterator] = function* () {
    Object.defineProperty(obj, "a", { get() { return 1; }, configurable: true });
    yield 1;
};

function f(set, o) {
    let x = o.a;
    let arr = [...set];
    return o.a;
}
noInline(f);

let plainSet = new Set([1]);
for (let i = 0; i < testLoopCount; i++)
    f(plainSet, { a: 1.1 });

obj = { a: 1.1 };
shouldBe(f(setWithOwnIterator, obj), 1);
