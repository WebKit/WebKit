function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, expectedError) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}!`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

const ITER_COUNT = 1e6;
const ITER_THROWS = ITER_COUNT / 2;

var nextThrows = {
    nextCount: 0,
    returnCount: 0,
    [Symbol.iterator]() {
        return {
            next: () => {
                this.nextCount++;
                if (this.nextCount === ITER_THROWS)
                    throw new Error("foo");

                return { done: false };
            },
            return: () => {
                this.returnCount++;
                return {};
            },
        };
    },
};

shouldThrow(() => {
    for (var i = 0; i < ITER_COUNT; i++)
        var [_] = nextThrows;
}, "Error: foo");

shouldBe(nextThrows.nextCount, ITER_THROWS);
shouldBe(nextThrows.returnCount, ITER_THROWS - 1);


var nextReturnsPrimitive = {
    nextCount: 0,
    returnCount: 0,
    [Symbol.iterator]() {
        return {
            next: () => {
                this.nextCount++;
                if (this.nextCount === ITER_THROWS)
                    return 1;

                return { done: false };
            },
            return: () => {
                this.returnCount++;
                return {};
            },
        };
    },
};

shouldThrow(() => {
    for (var i = 0; i < ITER_COUNT; i++)
        var [_] = nextReturnsPrimitive;
}, "TypeError: Iterator result interface is not an object.");

shouldBe(nextReturnsPrimitive.nextCount, ITER_THROWS);
shouldBe(nextReturnsPrimitive.returnCount, ITER_THROWS - 1);


var doneGetterThrows = {
    nextCount: 0,
    returnCount: 0,
    [Symbol.iterator]() {
        return {
            next: () => {
                this.nextCount++;
                if (this.nextCount === ITER_THROWS) {
                    return {
                        get done() { throw new Error("bar"); },
                    };
                }

                return { done: false };
            },
            return: () => {
                this.returnCount++;
                return {};
            },
        };
    },
};

shouldThrow(() => {
    for (var i = 0; i < ITER_COUNT; i++)
        var [x] = doneGetterThrows;
}, "Error: bar");

shouldBe(doneGetterThrows.nextCount, ITER_THROWS);
shouldBe(doneGetterThrows.returnCount, ITER_THROWS - 1);
