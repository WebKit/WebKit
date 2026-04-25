function shouldThrowTypeError(func, messagePrefix) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError!');

    if (!error.message.startsWith(messagePrefix))
        throw new Error('TypeError has wrong message!');
}

function shouldThrowExactly(func, expectedError) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (error !== expectedError)
        throw new Error(`Expected ${errorType.name}!`);
}

shouldThrowTypeError(() => ([...1]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');
shouldThrowTypeError(() => ([...undefined]), 'Spread syntax requires ...iterable not be null or undefined');
shouldThrowTypeError(() => ([...null]), 'Spread syntax requires ...iterable not be null or undefined');
shouldThrowTypeError(() => ([...3.14]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');
shouldThrowTypeError(() => ([.../a/g]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');
shouldThrowTypeError(() => ([...{}]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');
shouldThrowTypeError(() => ([...{a:[].join()}]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');

function f() {}
shouldThrowTypeError(() => ([...f]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');
shouldThrowTypeError(() => ([...[...() => undefined]]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');

class C {}
shouldThrowTypeError(() => ([...new C()]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');
shouldThrowTypeError(() => ([...C]), 'Spread syntax requires ...iterable[Symbol.iterator] to be a function');

const myErr = new Error('Custom Error');
shouldThrowExactly(() => [...{ [Symbol.iterator]() { throw myErr; }}], myErr);

let iteratorCount = 0;
let toStringCount = 0;
const myIterable = {
    [Symbol.iterator]() {
        iteratorCount += 1;
        return {
        next() {
                return { done: true };
            }
        };
    },
    toString() {
        toStringCount += 1;
        return "Iterable";
    }
};

function assertEqual(a, b) {
    if (a !== b)
        throw new Error(`${a} !== ${b}`);
}

[...myIterable];
assertEqual(iteratorCount, 1);
assertEqual(toStringCount, 0);

