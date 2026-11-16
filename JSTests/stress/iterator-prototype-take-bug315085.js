function shouldThrow(caseName, fn, expectedErrorCtor, expectedErrorMessage) {
    if (!caseName)
        throw new Error(`must specify test case name`);

    const expected = `${expectedErrorCtor.name}(${expectedErrorMessage})`;
    try {
        fn();
        throw new Error(`${caseName}: Expected to throw ${expected}, but succeeded`);
    } catch (e) {
        const actual = `${e.name}(${e.message})`;
        if (!(e instanceof expectedErrorCtor) || e.message !== expectedErrorMessage)
            throw new Error(`${caseName}: Expected ${expected} but got ${actual}`);
    }
}

function shouldNotThrow(fn, caseName) {
    if (!caseName)
        throw new Error(`must specify message`);

    try {
        fn();
    } catch (e) {
        const actual = `${e.name}(${e.message})`;
        throw new Error(`${caseName}: Expected not thrown but got ${actual}`);
    }
}

function shouldBe(a, b, testname) {
    if (a !== b)
        throw new Error(`${testname}: Expected ${b} but got ${a}`);
}

class TestIterator extends Iterator {
    value = 0;
    isClosed = false;
    isDone = false;
    constructor(max = 3) {
        super();
        if (max < 0) {
            throw new RangeError('max must be >= 0');
        }
        this.max = +max;
    }
    next() {
        const value = this.value;
        if (value > this.max) {
            this.isDone = true;
            return {
                done: true,
                value: undefined,
            };
        }

        this.value += 1;
        return {
            done: false,
            value,
        };
    }
    return() {
        this.isClosed = true;
        return {
            done: true,
            value: undefined,
        };
    }
}

function test(target, limit) {
    const newIter = Iterator.prototype.take.call(target, limit);
    return newIter;
}

{
    const validUpperLowerBoundArgumentTestCases = [
        [Number.POSITIVE_INFINITY, ],
        [Number.MAX_SAFE_INTEGER, `Number.MAX_SAFE_INTEGER`],
        [0, ],
        [0.0, `0.0`],
    ];

    for (const [value, name] of validUpperLowerBoundArgumentTestCases) {
        const label = name ?? String(name);
        const iter = new TestIterator();

        shouldNotThrow(() => {
            test(iter, value);
        }, `${label}: should be valid for the 1st argument`);
        shouldBe(iter.isClosed, false, `${label}: the iterator should not be closed`);
    }
}

{
    const invalidOutOfRangeArgumentTestCases = [
        [Number.MAX_VALUE, `Number.MAX_VALUE`],
        [Number.MAX_SAFE_INTEGER + 1, `Number.MAX_SAFE_INTEGER + 1`],
        [-1, ],
        [Number.NEGATIVE_INFINITY, ],
    ];

    for (const [value, name] of invalidOutOfRangeArgumentTestCases) {
        const label = name ?? String(name);
        const iter = new TestIterator();

        shouldThrow(`${label}: the 1st arg is out of range`, () => {
            test(iter, value);
        }, RangeError, `Iterator.prototype.take argument must be non-negative safe integer or +Infinity`);

        shouldBe(iter.isClosed, true, `${label}: the iterator should be closed`);
    }
}