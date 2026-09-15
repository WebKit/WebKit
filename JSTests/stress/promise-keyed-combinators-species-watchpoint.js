//@ requireOptions("--usePromiseAllKeyed=1")

// Fires the @@species-related watchpoints: pollutes Promise[Symbol.species] and per-promise
// "constructor" so the fast path's isThenFastAndNonObservable / promiseSpeciesConstructor checks
// fail and elements fall back to the real |then| call. Must stay a separate file: the pollution
// permanently disables these fast paths in this VM.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

// A promise whose own "constructor" points elsewhere: speciesConstructor observably consulted,
// element still settles correctly through the generic then path.
{
    class Sub extends Promise { }
    let constructorGets = 0;
    let promise = Promise.resolve("v");
    Object.defineProperty(promise, "constructor", {
        configurable: true,
        get() { constructorGets++; return Sub; },
    });
    let result;
    Promise.allKeyed({ p: promise }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result.p, "v");
    shouldBe(constructorGets >= 1, true);
}

// Pollute Promise[Symbol.species] with an accessor returning a subclass: every promise element in
// both combinators now routes through the observable then path; results stay correct.
{
    let speciesGets = 0;
    class Sub extends Promise { }
    Object.defineProperty(Promise, Symbol.species, {
        configurable: true,
        get() { speciesGets++; return Sub; },
    });

    let result;
    Promise.allKeyed({ a: Promise.resolve(1), b: Promise.resolve(2), c: 3 }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBeArray(Reflect.ownKeys(result), ["a", "b", "c"]);
    shouldBe(result.a, 1);
    shouldBe(result.b, 2);
    shouldBe(result.c, 3);
    shouldBe(speciesGets >= 1, true);

    let settled;
    Promise.allSettledKeyed({ ok: Promise.resolve("v"), ng: Promise.reject("r") }).then((value) => { settled = value; });
    drainMicrotasks();
    shouldBe(settled.ok.status, "fulfilled");
    shouldBe(settled.ok.value, "v");
    shouldBe(settled.ng.status, "rejected");
    shouldBe(settled.ng.reason, "r");
}

// Species returning the base Promise still works (fast tier may or may not re-engage; behavior
// must be identical either way).
{
    Object.defineProperty(Promise, Symbol.species, {
        configurable: true,
        get() { return Promise; },
    });
    let result;
    Promise.allKeyed({ x: Promise.resolve("x") }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result.x, "x");
}

// Rejection ordering is preserved under species pollution.
{
    let reason;
    let rejectB;
    Promise.allKeyed({
        a: Promise.resolve(1),
        b: new Promise((resolve, reject) => { rejectB = reject; }),
    }).catch((error) => { reason = error; });
    rejectB("late");
    drainMicrotasks();
    shouldBe(reason, "late");
}
