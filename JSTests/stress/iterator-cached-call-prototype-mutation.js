function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try {
        func();
    } catch (error) {
        threw = true;
        if (errorType && !(error instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${error.constructor.name}`);
    }
    if (!threw)
        throw new Error("Expected function to throw");
}

{
    function* generator() {
        yield 1;
        yield 2;
        yield 3;
    }
    const iter = generator();
    let called = 0;
    iter.forEach((element) => {
        called++;
        if (element === 1) {
            iter.next = function () {
                return { done: false, value: 3 };
            }
        }
    });
    shouldBe(called, 3);
}

{
    function* generator() {
        yield 100;
        yield 200;
    }
    const iter = generator();
    const originalNext = iter.next;
    let calledCount = 0;
    iter.next = function() {
        calledCount++;
        if (calledCount <= 2) {
            return originalNext.call(iter);
        } else {
            throw new Error("Exception during iteration");
        }
    };
    shouldThrow(() => {
        iter.map(x => x * 2).toArray();
    }, Error);
    shouldBe(calledCount, 3);
}

{
    const iter = {
        [Symbol.iterator]() { return this; },
        next() {
            return "not an object";
        }
    };
    shouldThrow(() => {
        iter.toArray();
    }, TypeError);
}

