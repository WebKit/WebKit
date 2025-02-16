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
    let returnCount = 0;

    function OurError() {}

    var iterator = {
      [Symbol.iterator]() {
        return {
          next() {
            return { value: 1, done: false };
          },
          throw() {
            return {
              value: Promise.reject(new OurError()),
              done: false
            };
          },
          return() {
            returnCount += 1;
          }
        };
      }
    };

    async function* asyncIterator() {
      return yield* iterator;
    }

    let iter = asyncIterator();

    shouldThrowAsync(async () => {
        await iter.next();
        await iter.throw();
    }, OurError);
    drainMicrotasks();

    shouldBe(returnCount, 1);
}

