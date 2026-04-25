//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

// %AsyncIteratorPrototype%[@@asyncDispose] step 6.a:
//   Let result be Completion(Call(return.[[Value]], O, « undefined »)).
// The return method must be called with exactly one argument (undefined),
// matching for-await-of abrupt completion semantics. The sync
// %IteratorPrototype%[@@dispose] calls return with no arguments (« »).

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
    shouldBe(argsLength, 1);
    shouldBe(arg0, undefined);
}

// Sync @@dispose should still call return with no arguments
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
    shouldBe(argsLength, 1);
}
