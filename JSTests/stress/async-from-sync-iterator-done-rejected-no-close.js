function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

var error = new Error("rejected");

function makeIterable(done) {
    var record = { returnCount: 0 };
    record.iterable = {
        [Symbol.iterator]() {
            return {
                next() {
                    return { value: Promise.reject(error), done };
                },
                return() {
                    record.returnCount++;
                    return {};
                }
            };
        }
    };
    return record;
}

async function test() {
    var record = makeIterable(true);
    try {
        for await (var v of record.iterable) { }
        throw new Error("should not reach");
    } catch (e) {
        shouldBe(e, error);
    }
    shouldBe(record.returnCount, 0);

    record = makeIterable(false);
    try {
        for await (var v of record.iterable) { }
        throw new Error("should not reach");
    } catch (e) {
        shouldBe(e, error);
    }
    shouldBe(record.returnCount, 1);
}

var finished = false;
var failure;
test().then(() => { finished = true; }, (e) => { failure = e; });
drainMicrotasks();
if (failure)
    throw failure;
shouldBe(finished, true);
