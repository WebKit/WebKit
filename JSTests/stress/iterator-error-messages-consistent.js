function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function testIteratorError(value, expectedError) {
    function iterate() {
        for (let x of value) { }
    }
    
    shouldThrow(iterate, expectedError);
}

function runTest(value, expectedError) {
    for (let i = 0; i < testLoopCount; i++) {
        testIteratorError(value, expectedError);
    }
}

runTest(null, "TypeError: null is not an object (evaluating 'value')");
runTest(undefined, "TypeError: undefined is not an object (evaluating 'value')");
runTest(0, "TypeError: number is not iterable");
runTest(42, "TypeError: number is not iterable");
runTest(true, "TypeError: true is not iterable");
runTest(false, "TypeError: false is not iterable");
runTest(Symbol("test"), "TypeError: value is not iterable");
runTest({}, "TypeError: {} is not iterable");
runTest({ a: 1 }, "TypeError: {} is not iterable");

function testFromEntries(value, expectedError) {
    function test() {
        return Object.fromEntries(value);
    }
    shouldThrow(test, expectedError);
}

for (let i = 0; i < testLoopCount; i++) {
    testFromEntries(0, "TypeError: number is not iterable");
    testFromEntries(true, "TypeError: true is not iterable");
    testFromEntries(null, "TypeError: null is not an object");
}

function testDestructuring(value, expectedError) {
    function test() {
        let [x] = value;
    }
    shouldThrow(test, expectedError);
}

for (let i = 0; i < testLoopCount; i++) {
    testDestructuring(42, "TypeError: number is not iterable");
    testDestructuring(null, "TypeError: null is not an object (evaluating 'value')");
}

function testSpread(value, expectedError) {
    function test() {
        return [...value];
    }
    shouldThrow(test, expectedError);
}

for (let i = 0; i < testLoopCount; i++) {
    testSpread(0, "TypeError: Spread syntax requires ...iterable[Symbol.iterator] to be a function");
    testSpread(null, "TypeError: Spread syntax requires ...iterable not be null or undefined");
    testSpread(false, "TypeError: Spread syntax requires ...iterable[Symbol.iterator] to be a function");
}