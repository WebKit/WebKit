function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var symbol = Symbol('Cocoa');

var object = {
    [symbol]: 3,
    0: 0,
    hello: 2,
    1: 1,
};

var count = 0;

var tester = Object.defineProperties({}, {
    0: {
        set: () => {
            shouldBe(count++, 0);
        }
    },
    1: {
        set: () => {
            shouldBe(count++, 1);
        }
    },
    'hello': {
        set: () => {
            shouldBe(count++, 2);
        }
    },
    [symbol]: {
        set: () => {
            shouldBe(count++, 3);
        }
    },
});

Object.assign(tester, object);
