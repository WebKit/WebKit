//@ requireOptions("--useIteratorIncludes=1")

function sameValue(a, b, testname) {
    if (a !== b)
        throw new Error(`${testname}: Expected ${b} but got ${a}`);
}

function shouldThrow(caseName, fn, expectedErrorCtor, expectedErrorMessage) {
    if (!caseName) {
        throw new Error(`must specify test case name`);
    }

    try {
        fn();
        throw new Error(`${caseName}: Expected to throw, but succeeded`);
    } catch (e) {
        if (!(e instanceof expectedErrorCtor))
            throw new Error(`${caseName}: Expected to throw ${expectedErrorCtor.name} but got ${e.name}`);
        if (e.message !== expectedErrorMessage)
            throw new Error(`${caseName}: Expected ${expectedErrorCtor.name} with '${expectedErrorMessage}' but got '${e.message}'`);
    }
}

function callTestTargetFunction(iterator, searchElement, skippedElements) {
    return Iterator.prototype.includes.call(iterator, searchElement, skippedElements);
}

class TestIterator extends Iterator {
    value = 0;
    isClosed = false;
    isDone = false;
    constructor(max) {
        super();
        if (max < 0) {
            throw new RangeError('max must be >= 0');
        }
        this.max = max;
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

// without second argument

{
    const TEST_NAME = `simply include`;

    const gen = function* (num) {
        for (let i = 0; i <= num; ++i) {
            yield i;
        }
    };
    const iter = gen(3);
    sameValue(callTestTargetFunction(iter, 1), true, TEST_NAME);
}


{
    const TEST_NAME = `simply not include`;

    const gen = function* (len) {
        for (let i = 0; i < len; ++i) {
            yield i;
        }
    };
    const iter = gen(3);
    sameValue(callTestTargetFunction(iter, 4), false, TEST_NAME);
}

{
    const TEST_NAME = `iterator is empty`;

    const gen = function* () {};
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 1), false, TEST_NAME);
}

{
    const TEST_NAME = `Use SameValueZero comparison`;

    const gen = function* () {
        yield +0;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, -0), true, TEST_NAME);
}

{
    const TEST_NAME = `Use SameValueZero comparison, search NaN`;

    const gen = function* () {
        yield NaN;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, NaN), true, TEST_NAME);
}

{
    const TEST_NAME = `Use SameValueZero comparison, search Number.NaN`;

    const gen = function* () {
        yield NaN;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, Number.NaN), true, TEST_NAME);
}

{
    const TEST_NAME = `Use SameValueZero comparison, the iteraror yeild NaN but the searching value is anothor one.`;

    const gen = function* () {
        yield NaN;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 0), false, TEST_NAME);
}

{
    const TEST_NAME = `Use SameValueZero comparison, the iteraror yeild Number.NaN but the searching value is anothor one.`;

    const gen = function* () {
        yield Number.NaN;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 0), false, TEST_NAME);
}

{
    const TEST_NAME = `iterator should be closed`;

    const gen = function* () {
        yield 0;
        yield 1;
        yield 2;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 1), true, `${TEST_NAME}: consume once`);
    sameValue(callTestTargetFunction(iter, 2), false, `${TEST_NAME}: 2nd iteration on consumed iterator would be nothing`);
}

{
    const TEST_NAME = `do not iterate all, the actual should be expected`;

    class Unreachable extends Error {
        constructor(message) {
            super(message);
            this.name = new.target.name;
        }
    }
    const gen = function* () {
        yield 0;
        throw new Unreachable("do not iterate all after the target was found");
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 0), true, TEST_NAME);
}

{
    const TEST_NAME = 'calling .includes() should close the iterator if the searched was found';

    const iter = new TestIterator(5);
    sameValue(callTestTargetFunction(iter, 4), true, `${TEST_NAME}: call`);
    sameValue(iter.isDone, false, `${TEST_NAME}: iterator should not be done`);
    sameValue(iter.isClosed, true, `${TEST_NAME}: iterator should be closed`);
}

{
    const TEST_NAME = 'calling .includes() should not close the iterator if no matched';

    const iter = new TestIterator(5);
    sameValue(callTestTargetFunction(iter, -1), false, `${TEST_NAME}: call`);
    sameValue(iter.isDone, true, `${TEST_NAME}: iterator should be done`);
    sameValue(iter.isClosed, false, `${TEST_NAME}: iterator should not be closed`);
}

// with second argument

{
    const TEST_NAME = `call with 2nd arg: simply include`;

    const gen = function* (len) {
        for (let i = 0; i < len; ++i) {
            yield i;
        }
    };
    const iter = gen(3);
    sameValue(callTestTargetFunction(iter, 2, 2), true, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg is 0: simply include`;

    const gen = function* (len) {
        for (let i = 0; i < len; ++i) {
            yield i;
        }
    };
    const iter = gen(3);
    sameValue(callTestTargetFunction(iter, 2, 2), true, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: simply include but the target is in skipped range`;

    const gen = function* (len) {
        for (let i = 0; i < len; ++i) {
            yield i;
        }
    };
    const iter = gen(3);
    sameValue(callTestTargetFunction(iter, 2, 3), false, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: simply include but the skipped range is over than the iterator length`;

    const iter = new TestIterator(3);
    sameValue(callTestTargetFunction(iter, 2, 100), false, TEST_NAME);
    sameValue(iter.isDone, true, `${TEST_NAME}: iterator should be done properly`);
}

{
    const TEST_NAME = `call with 2nd arg: simply not include`;

    const gen = function* (len) {
        for (let i = 0; i < len; ++i) {
            yield i;
        }
    };
    const iter = gen(3);
    sameValue(callTestTargetFunction(iter, 4, 0), false, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: iterator is empty`;

    const gen = function* () {};
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 1, 1), false, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: Use SameValueZero comparison`;

    const gen = function* () {
        yield -1;
        yield +0;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, -0, 1), true, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: Use SameValueZero comparison, search NaN`;

    const gen = function* () {
        yield -1;
        yield NaN;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, NaN, 1), true, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: Use SameValueZero comparison, search NaN but the iterator will not return NaN`;

    const gen = function* () {
        yield -1;
        yield 0;
        yield 1;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, NaN, 1), false, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: Use SameValueZero comparison, search Number.NaN`;

    const gen = function* () {
        yield -1;
        yield NaN;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, Number.NaN, 1), true, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: Use SameValueZero comparison, search Number.NaN but the iterator will not return NaN`;

    const gen = function* () {
        yield -1;
        yield 0;
        yield 1;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, Number.NaN, 1), false, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: Use SameValueZero comparison, the iterator yield NaN but the searching value is another one`;

    const gen = function* (limit) {
        for (let i = 0; i < limit; ++i) {
            yield NaN;
        }
    };
    const iter = gen(10);
    sameValue(callTestTargetFunction(iter, 0, 1), false, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: Use SameValueZero comparison, the iterator yield Number.NaN but the searching value is another one`;

    const gen = function* (limit) {
        for (let i = 0; i < limit; ++i) {
            yield NaN;
        }
    };
    const iter = gen(10);
    sameValue(callTestTargetFunction(iter, 0, 1), false, TEST_NAME);
}

{
    const TEST_NAME = `call with 2nd arg: iterator should be closed`;

    const gen = function* () {
        yield 0;
        yield 1;
        yield 2;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 1, 1), true, `${TEST_NAME}: consume once`);
    sameValue(callTestTargetFunction(iter, 2, 1), false, `${TEST_NAME}: 2nd iteration on closed iterator would be nothing`);
}

{
    const TEST_NAME = `call with 2nd arg but skipped all: iterator should be closed`;

    const gen = function* () {
        yield 0;
        yield 1;
        yield 2;
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 1, 100), false, `${TEST_NAME}: consume once`);
    sameValue(callTestTargetFunction(iter, 2, 100), false, `${TEST_NAME}: 2nd iteration on closed iterator would be nothing`);
}

{
    const TEST_NAME = `call with 2nd arg: do not iterate all`;

    class Unreachable extends Error {
        constructor(message) {
            super(`unreachable: ${message}`);
            this.name = new.target.name;
        }
    }
    const gen = function* () {
        yield -1;
        yield 0;
        throw new Unreachable(`${TEST_NAME}: after the searched was found`);
    };
    const iter = gen();
    sameValue(callTestTargetFunction(iter, 0, 1), true, `${TEST_NAME}: the searched was found`);
}

{
    const TEST_NAME = `call with 2nd arg: skipped range throw something`;

    class TestIterator extends Iterator {
        value = 0;
        isClosed = false;
        isDone = false;
        constructor(max) {
            super();
            this.max = max;
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

            throw new Error('this is in skipped range!');
        }
        return() {
            this.isClosed = true;
            return {
                done: true,
                value: undefined,
            };
        }
    }

    const iter = new TestIterator(3);
    shouldThrow(TEST_NAME, () => {
        callTestTargetFunction(iter, 1, 100);
    }, Error, 'this is in skipped range!');
    sameValue(iter.isDone, false, `${TEST_NAME}: iterator should not be done`);
    sameValue(iter.isClosed, false, `${TEST_NAME}: iterator should not be closed`);
}

{
    const TEST_NAME = `call with 2nd arg: calling .includes() should close the iterator`;

    const iter = new TestIterator(5);
    sameValue(callTestTargetFunction(iter, 4, 1), true, `${TEST_NAME}: calling .includes()`);
    sameValue(iter.isDone, false, `${TEST_NAME}: iterator should not be done`);
    sameValue(iter.isClosed, true, `${TEST_NAME}: iterator should be closed`);
}

{
    const TEST_NAME = `call with 2nd arg but skip everything`;

    const iter = new TestIterator(5);
    sameValue(callTestTargetFunction(iter, 4, 100), false, `${TEST_NAME}: calling .includes()`);
    sameValue(iter.isDone, true, `${TEST_NAME}: iterator should be done`);
    sameValue(iter.isClosed, false, `${TEST_NAME}: iterator should not be closed`);
}

{
    const valid2ndArgument = [
        [Number.POSITIVE_INFINITY, `+Infinity`],
        [Number.MAX_SAFE_INTEGER, `Number.MAX_SAFE_INTEGER`],
        [Number.MAX_SAFE_INTEGER + 1, `(Number.MAX_SAFE_INTEGER + 1)`],
        [Number.MAX_VALUE, `Number.MAX_VALUE`],
    ];

    for (const [skippedElements, label] of valid2ndArgument) {
        const testName = `call on short iterator with 2nd arg (large number): ${label}`;
        const iter = new TestIterator(0);
        sameValue(callTestTargetFunction(iter, 0, skippedElements), false, `${testName}: calling .includes()`);
        sameValue(iter.isDone, true, `${testName}: iterator should be done`);
        sameValue(iter.isClosed, false, `${testName}: iterator should not be closed`);
    }
}

// invalid

{
    const invalidIterators = [
        [1, ],
        [1n, `1n`],
        [true, ],
        [false, ],
        [null, ],
        [undefined, ],
        [Symbol("symbol"), ],
    ];
    for (const [invalidIterator, label] of invalidIterators) {
        const testName = label ?? String(invalidIterator);
        shouldThrow(`|this| is invalid: ${testName}`, function () {
            Iterator.prototype.includes.call(invalidIterator);
        }, TypeError, "Iterator.prototype.includes requires that |this| be an Object.");
    }
}

{
    const invalidSkippedElements = [
        ['', ],
        ['1', `'1'`],
        [true, ],
        [false, ],
        [null, ],
        [1.2, ],
        [-1.2, ],
        [1n, `1n`],
        [-1n, `-1n`],
        [new Number(0), `new Number`],
        [NaN, 'NaN'],
        [Number.NaN, 'Number.NaN'],
        [Number.MIN_VALUE, `Number.MIN_VALUE`],
    ];

    for (const [skip, label] of invalidSkippedElements) {
        const testName = label ?? String(skip);
        const validIter = new TestIterator(3);
        shouldThrow(`the 2nd arg is invalid: ${testName}`, function () {
            callTestTargetFunction(validIter, null, skip);
        }, TypeError, "Iterator.prototype.includes requires that the second argument is a non-negative integral Number or Infinity.");
        sameValue(validIter.isClosed, true, 'the given iterator should be closed.');
    }
}

{
    const invalidSkippedElements = [
        [-1, ],
        [Number.NEGATIVE_INFINITY, ],
        [Number.MIN_SAFE_INTEGER, `Number.MIN_SAFE_INTEGER`],
    ];
    for (const [skip, label] of invalidSkippedElements) {
        const testName = label ?? String(skip);
        const validIter = new TestIterator(3);
        shouldThrow(`the 2nd arg is not positive integer: ${String(testName)}`, function () {
            callTestTargetFunction(validIter, null, skip);
        }, RangeError, "Iterator.prototype.includes requires that the second argument is a non-negative integral Number or Infinity.");
        sameValue(validIter.isClosed, true, 'the given iterator should be closed.');
    }
}