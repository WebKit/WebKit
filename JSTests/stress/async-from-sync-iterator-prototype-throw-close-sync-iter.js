function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}


function shouldThrowAsync(run, errorType, message) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();
    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
}

{
    // When sync-iterator's `throw` is null, `AsyncFromSyncIterator#throw` throws TypeError
    let calledReturn = 0;
    let calledThrowGetter = 0;

    const syncIterator = {
        [Symbol.iterator]() {
            return this;
        },
        next() {
          return {value: 1, done: false};
        },
        return() {
            calledReturn++;
            return { done: true, value: 1 };
        },
        get throw() {
            calledThrowGetter++;
            return null;
        }
    };

    const asyncIterator = (async function* () {
        yield* syncIterator;
    })();

    function OurError() {}

    asyncIterator.next();
    drainMicrotasks();

    shouldThrowAsync(async () => {
        await asyncIterator.throw(new OurError());
    }, TypeError);
    drainMicrotasks();

    shouldBe(calledReturn, 1);
    shouldBe(calledThrowGetter, 1);
}

{
    // When sync-iterator's `throw` is undefined, `AsyncFromSyncIterator#throw` throws TypeError
    let calledReturn = 0;
    let calledThrowGetter = 0;

    const syncIterator = {
        [Symbol.iterator]() {
            return this;
        },
        next() {
          return {value: 1, done: false};
        },
        return() {
            calledReturn++;
            return { done: true, value: 1 };
        },
        get throw() {
            calledThrowGetter++;
            return undefined;
        }
    };

    const asyncIterator = (async function* () {
        yield* syncIterator;
    })();

    function OurError() {}

    asyncIterator.next();
    drainMicrotasks();

    shouldThrowAsync(async () => {
        await asyncIterator.throw(new OurError());
    }, TypeError);
    drainMicrotasks();

    shouldBe(calledReturn, 1);
    shouldBe(calledThrowGetter, 1);
}

{
    // When sync-iterators' `return` getter throws an error, `AsyncFromSyncIterator#throw` throw the error.
    let calledReturnGetter = 0;
    let calledThrowGetter = 0;

    function OurError() {}
    function NotOurError() {}

    const syncIterator = {
        [Symbol.iterator]() {
            return this;
        },
        next() {
          return {value: 1, done: false};
        },
        get return() {
            calledReturnGetter++;
            throw new OurError();
        },
        get throw() {
            calledThrowGetter++;
            return undefined;
        }
    };

    const asyncIterator = (async function* () {
        yield* syncIterator;
    })();

    asyncIterator.next();
    drainMicrotasks();

    shouldThrowAsync(async () => {
        await asyncIterator.throw(new NotOurError());
    }, OurError);
    drainMicrotasks();

    shouldBe(calledReturnGetter, 1);
    shouldBe(calledThrowGetter, 1);
}

{
    // When sync-iterator's `return` returns non-object, `AsyncFromSyncIterator#throw` throw TypeError.
    let calledReturn = 0;
    let calledThrowGetter = 0;

    const syncIterator = {
        [Symbol.iterator]() {
            return this;
        },
        next() {
          return { value: 1, done: false };
        },
        return() {
            calledReturn++;
            return 2;
        },
        get throw() {
            calledThrowGetter++;
            return null;
        }
    };

    const asyncIterator = (async function* () {
        yield* syncIterator;
    })();

    function OurError() {}

    asyncIterator.next();
    drainMicrotasks();

    shouldThrowAsync(async () => {
        await asyncIterator.throw(new OurError());
    }, TypeError);
    drainMicrotasks();

    shouldBe(calledReturn, 1);
    shouldBe(calledThrowGetter, 1);
}
