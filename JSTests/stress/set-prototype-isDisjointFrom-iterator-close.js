function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    if (actual.length !== expected.length)
        throw new Error('array length mismatch: ' + actual.length + ' vs ' + expected.length);
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error('array element mismatch at index ' + i + ': ' + actual[i] + ' vs ' + expected[i]);
    }
}

{
    let log = [];

    let keysIter = {
        next() {
            log.push("next");
            return {done: false, value: 2};
        },
        return() {
            log.push("return");
            return {
                get value() { throw new Error("Unexpected call to |value| getter"); },
                get done() { throw new Error("Unexpected call to |done| getter"); },
            };
        }
    };

    let setLike = {
        size: 1,
        has() {
            throw new Error("Unexpected call to |has| method");
        },
        keys() {
            return keysIter;
        },
    };

    let result = new Set([1, 2, 3]).isDisjointFrom(setLike);
    shouldBe(result, false);
    shouldBeArray(log, ["next", "return"]);
}

{
    let returnCalled = false;

    let keysIter = {
        next() {
            return {done: true};
        },
        return() {
            returnCalled = true;
            return {};
        }
    };

    let setLike = {
        size: 0,
        has() {
            throw new Error("Unexpected call to |has| method");
        },
        keys() {
            return keysIter;
        },
    };

    let result = new Set([1, 2, 3]).isDisjointFrom(setLike);
    shouldBe(result, true);
    shouldBe(returnCalled, false);
}
