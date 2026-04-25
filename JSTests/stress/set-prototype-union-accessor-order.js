function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function shouldBeArray(actual, expected) {
    if (actual.length !== expected.length)
        throw new Error(
            `bad array length: ${actual.length}, expected: ${expected.length}`,
        );
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error(
                `bad value at index ${i}: ${actual[i]}, expected: ${expected[i]}`,
            );
    }
}

let observedOrder = [];

let setLikeObject = {
    size: 3,
    has: function () {
        observedOrder.push('has()');
        return false;
    },
    keys: function () {
        observedOrder.push('keys()');
        let values = ['a', 'b', 'c'];
        let index = 0;
        return {
            get next() {
                observedOrder.push('getting next');
                return function () {
                    observedOrder.push('calling next');
                    if (index < values.length) {
                        return {
                            get done() {
                                observedOrder.push('getting done');
                                return false;
                            },
                            get value() {
                                observedOrder.push('getting value');
                                return values[index++];
                            },
                        };
                    }
                    return {
                        get done() {
                            observedOrder.push('getting done');
                            return true;
                        },
                    };
                };
            },
        };
    },
};

let set = new Set([1, 2]);
let result = set.union(setLikeObject);

shouldBe(result.size, 5);
shouldBe(result.has(1), true);
shouldBe(result.has(2), true);
shouldBe(result.has('a'), true);
shouldBe(result.has('b'), true);
shouldBe(result.has('c'), true);

let expectedOrder = [
    'keys()',
    'getting next',
    'calling next',
    'getting done',
    'getting value',
    'calling next',
    'getting done',
    'getting value',
    'calling next',
    'getting done',
    'getting value',
    'calling next',
    'getting done',
];

shouldBeArray(observedOrder, expectedOrder);
