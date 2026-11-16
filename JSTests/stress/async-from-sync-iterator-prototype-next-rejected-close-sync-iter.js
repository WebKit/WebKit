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
    let finallyCount = 0;

    function OurError() {}

    function* iterator() {
      try {
        yield Promise.reject(new OurError());
      } finally {
        finallyCount += 1;
      }
    }

    shouldThrowAsync(async () => {
        for await (const _ of iterator());
    }, OurError);
    drainMicrotasks();

    shouldBe(finallyCount, 1);
}

{
    let returnCount = 0;

    function OurError() {}

    const syncIterator = {
      [Symbol.iterator]() {
        return {
          next() {
            return { value: Promise.reject(new OurError()), done: false };
          },
          return() {
            returnCount += 1;
          }
        };
      }
    };

    shouldThrowAsync(async () => {
        for await (const _ of syncIterator);
    }, OurError);
    drainMicrotasks();

    shouldBe(returnCount, 1);
}

{
    let finallyCount = 0;

    function OurError() {}

    function* iterator() {
      try {
        yield Promise.reject(new OurError());
      } finally {
        finallyCount += 1;
      }
    }

    async function* asyncIterator() {
      yield* iterator()
    }

    shouldThrowAsync(async () => {
        await asyncIterator().next();
    }, OurError);
    drainMicrotasks();

    shouldBe(finallyCount, 1);

}

{
    let returnCount = 0;

    function OurError() {}

    const syncIterator = {
      [Symbol.iterator]() {
        return {
          next() {
            return { value: Promise.reject(new OurError()), done: false };
          },
          return() {
            returnCount += 1;
          }
        };
      }
    };

    async function* asyncIterator() {
      yield* syncIterator;
    }

    shouldThrowAsync(async () => {
        await asyncIterator().next();
    }, OurError);
    drainMicrotasks();

    shouldBe(returnCount, 1);
}

