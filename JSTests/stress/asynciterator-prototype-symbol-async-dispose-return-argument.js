//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

// %AsyncIteratorPrototype%[@@asyncDispose] step 6.a (per tc39/proposal-explicit-resource-management#273):
//   Let result be Completion(Call(return.[[Value]], O)).
// The return method must be called with no arguments, matching the sync
// %IteratorPrototype%[@@dispose], which also calls return with no arguments (« »).

async function* ag() {}
const AsyncIteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf(Object.getPrototypeOf(ag())));

{
    let argsLength, arg0;
    const iter = Object.create(AsyncIteratorPrototype);
    iter.return = function(...args) {
        argsLength = args.length;
        arg0 = args[0];
        return {};
    };
    iter[Symbol.asyncDispose]();
    drainMicrotasks();
    shouldBe(argsLength, 0);
    shouldBe(arg0, undefined);
}

// Sync @@dispose should also call return with no arguments
{
    const IteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));
    let argsLength;
    const iter = Object.create(IteratorPrototype);
    iter.return = function(...args) {
        argsLength = args.length;
        return {};
    };
    iter[Symbol.dispose]();
    shouldBe(argsLength, 0);
}

// Observable via `arguments` object (no rest)
{
    let argsLength;
    const iter = Object.create(AsyncIteratorPrototype);
    iter.return = function() {
        argsLength = arguments.length;
        return {};
    };
    iter[Symbol.asyncDispose]();
    drainMicrotasks();
    shouldBe(argsLength, 0);
}

// Async generator's own return is also called with no arguments via @@asyncDispose
{
    let argsLength;
    async function* gen() {
        try {
            yield 1;
        } finally {
            // for-await-of abrupt completion would pass undefined here, but
            // @@asyncDispose passes nothing, so the rest binding is empty.
        }
    }
    const it = gen();
    const origReturn = AsyncIteratorPrototype.return;
    // Override return on the instance to observe the argument count.
    it.return = function(...args) {
        argsLength = args.length;
        return Promise.resolve({ value: undefined, done: true });
    };
    it[Symbol.asyncDispose]();
    drainMicrotasks();
    shouldBe(argsLength, 0);
}
