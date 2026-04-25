function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Test with Int32 array
function testInt32ArraySinkingWithConditionalWrite(shouldInitialize) {
    let arr = new Array(2);

    if (shouldInitialize) {
        arr[0] = 42;
    }

    return arr[0];
}
noInline(testInt32ArraySinkingWithConditionalWrite);

// Test with Double array
function testDoubleArraySinkingWithConditionalWrite(shouldInitialize) {
    let arr = new Array(2);

    if (shouldInitialize) {
        arr[0] = 42.5;
    }

    return arr[0];
}
noInline(testDoubleArraySinkingWithConditionalWrite);

// Test with Contiguous (JSValue) array
function testContiguousArraySinkingWithConditionalWrite(shouldInitialize) {
    let arr = new Array(2);

    if (shouldInitialize) {
        arr[0] = {value: 42};
    }

    return arr[0];
}
noInline(testContiguousArraySinkingWithConditionalWrite);

for (let i = 0; i < testLoopCount; ++i) {
    // Test Int32 array - initialized path
    shouldBe(testInt32ArraySinkingWithConditionalWrite(true), 42);
    // Test Int32 array - uninitialized path (hole)
    shouldBe(testInt32ArraySinkingWithConditionalWrite(false), undefined);

    // Test Double array - initialized path
    shouldBe(testDoubleArraySinkingWithConditionalWrite(true), 42.5);
    // Test Double array - uninitialized path (hole)
    shouldBe(testDoubleArraySinkingWithConditionalWrite(false), undefined);

    // Test Contiguous array - initialized path
    let resultInitialized = testContiguousArraySinkingWithConditionalWrite(true);
    shouldBe(resultInitialized.value, 42);
    // Test Contiguous array - uninitialized path (hole)
    shouldBe(testContiguousArraySinkingWithConditionalWrite(false), undefined);
}
