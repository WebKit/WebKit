//@ requireOptions("--usePromiseAllKeyed=1")

// Exercises the generic (non-%Promise% constructor) path of Promise.allKeyed: subclasses and
// arbitrary constructors reached via .call. These do not fire any global watchpoints, so the
// fast path stays available to other tests in this file.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`bad error: ${String(error)}`);
    }
    if (!errorThrown)
        throw new Error("not thrown");
}

// Subclass end-to-end: returns a subclass instance and resolves with the keyed result.
{
    class MyPromise extends Promise { }
    let returned = MyPromise.allKeyed({ a: Promise.resolve(1), b: 2 });
    shouldBe(returned instanceof MyPromise, true);
    let result;
    returned.then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBe(result.a, 1);
    shouldBe(result.b, 2);
}

// Subclass with an overridden resolve: the override is called once per enumerable key and its
// transformation is observable in the result.
{
    class DoublingPromise extends Promise {
        static resolve(value) {
            return super.resolve(typeof value === "number" ? value * 2 : value);
        }
    }
    let result;
    DoublingPromise.allKeyed({ a: 10, b: 20, c: "s" }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result.a, 20);
    shouldBe(result.b, 40);
    shouldBe(result.c, "s");
}

// Custom constructor: NewPromiseCapability runs the executor, "resolve" is looked up once and
// called once per enumerable key, and the capability's resolve receives the keyed result object.
{
    let resolveGets = 0;
    let resolveCalls = [];
    let capabilityResolve;
    let capabilityReject;
    function CustomPromise(executor) {
        executor((value) => { capabilityResolve = value; }, (error) => { capabilityReject = error; });
    }
    Object.defineProperty(CustomPromise, "resolve", {
        configurable: true,
        get() {
            resolveGets++;
            return function (value) {
                resolveCalls.push(value);
                return Promise.resolve(value);
            };
        },
    });

    let input = { x: 1, y: Promise.resolve(2) };
    Object.defineProperty(input, "hidden", { value: 3, enumerable: false });
    let returned = Promise.allKeyed.call(CustomPromise, input);
    shouldBe(returned instanceof CustomPromise, true);
    drainMicrotasks();
    shouldBe(resolveGets, 1);
    shouldBe(resolveCalls.length, 2);
    shouldBe(resolveCalls[0], 1);
    shouldBe(typeof capabilityResolve, "object");
    shouldBe(Object.getPrototypeOf(capabilityResolve), null);
    shouldBe(capabilityResolve.x, 1);
    shouldBe(capabilityResolve.y, 2);
    shouldBe(capabilityReject, undefined);
}

// Non-callable constructor.resolve rejects with a TypeError, before inspecting |promises|.
{
    let rejected;
    function CustomPromise(executor) {
        executor(() => { }, (error) => { rejected = error; });
    }
    CustomPromise.resolve = 42;
    Promise.allKeyed.call(CustomPromise, "not an object");
    drainMicrotasks();
    shouldBe(rejected instanceof TypeError, true);
    shouldBe(String(rejected), "TypeError: Promise resolve is not a function");
}

// A throwing "resolve" getter on the constructor rejects the capability.
{
    let rejected;
    function CustomPromise(executor) {
        executor(() => { }, (error) => { rejected = error; });
    }
    Object.defineProperty(CustomPromise, "resolve", { get() { throw new Error("resolve getter"); } });
    Promise.allKeyed.call(CustomPromise, { a: 1 });
    drainMicrotasks();
    shouldBe(String(rejected), "Error: resolve getter");
}

// Non-constructor / bad-executor |this| values throw synchronously from NewPromiseCapability.
{
    shouldThrow(() => Promise.allKeyed.call({}, {}), "TypeError: argument is not a constructor");
    shouldThrow(() => Promise.allKeyed.call(function plain() { }, {}), "TypeError: executor did not take a resolve function");
}

// Slow-path resolve-element functions handed to a thenable: per-key distinct, spec-shaped, and
// only the first call per key counts.
{
    let onFulfilledFunctions = [];
    function CustomPromise(executor) {
        executor(() => { }, () => { });
    }
    CustomPromise.resolve = function (value) { return value; }; // Pass thenables through untouched.

    let capabilityResolve;
    function CollectingPromise(executor) {
        executor((value) => { capabilityResolve = value; }, () => { });
    }
    CollectingPromise.resolve = function (value) { return value; };

    let input = {
        first: { then(onFulfilled) { onFulfilledFunctions.push(onFulfilled); } },
        second: { then(onFulfilled) { onFulfilledFunctions.push(onFulfilled); } },
    };
    Promise.allKeyed.call(CollectingPromise, input);
    shouldBe(onFulfilledFunctions.length, 2);
    let [first, second] = onFulfilledFunctions;
    shouldBe(first !== second, true);
    shouldBe(first.length, 1);
    shouldBe(first.name, "");
    shouldBe(Object.prototype.hasOwnProperty.call(first, "prototype"), false);

    first("v1");
    first("ignored"); // [[AlreadyCalled]].
    second("v2");
    drainMicrotasks();
    shouldBe(Object.getPrototypeOf(capabilityResolve), null);
    shouldBe(capabilityResolve.first, "v1");
    shouldBe(capabilityResolve.second, "v2");
}

// Slow path with index keys mixed in.
{
    class MyPromise extends Promise { }
    let result;
    MyPromise.allKeyed({ 2: Promise.resolve("two"), name: Promise.resolve("n"), 0: "zero" }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBeArray(Reflect.ownKeys(result), ["0", "2", "name"]);
    shouldBe(result[0], "zero");
    shouldBe(result[2], "two");
    shouldBe(result.name, "n");
}

// Slow path rejection: capability reject receives the first rejection reason.
{
    class MyPromise extends Promise { }
    let reason;
    MyPromise.allKeyed({ a: Promise.resolve(1), b: Promise.reject("boom") }).catch((error) => { reason = error; });
    drainMicrotasks();
    shouldBe(reason, "boom");
}
