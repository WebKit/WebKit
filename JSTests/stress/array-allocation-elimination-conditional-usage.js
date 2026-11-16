function assert(condition, message) {
    if (!condition)
        throw new Error(message || "Assertion failed");
}

// Test conditional array usage - some branches use array, others don't
function testConditionalArrayUsage(useArray, writeValue) {
    const arr = new Array(5);

    if (useArray) {
        // Branch that uses the array
        arr[0] = writeValue;
        arr[2] = writeValue + 10;
        arr[4] = writeValue + 20;

        let sum = 0;
        for (let i = 0; i < 5; i++) {
            if (arr[i] !== undefined) {
                sum += arr[i];
            }
        }
        return sum;
    } else {
        // Branch that doesn't use the array at all
        return writeValue * 3 + 30; // Same result as the sum above when writeValue = 0
    }
}

// Test with nested conditionals
function testNestedConditionalArray(outer, inner, value) {
    const arr = new Array(3);

    if (outer) {
        arr[0] = value;

        if (inner) {
            arr[1] = value * 2;
            arr[2] = value * 3;
            return arr[0] + arr[1] + arr[2]; // value * 6
        } else {
            // Only use first element
            return arr[0]; // value
        }
    } else {
        // Don't use array at all
        return inner ? value * 6 : value;
    }
}

// Test with early return that skips array usage
function testEarlyReturnArray(shouldReturn, value) {
    const arr = new Array(4);

    if (shouldReturn) {
        return value * 42;
    }

    // This array usage should be eliminated when shouldReturn is true
    arr[0] = value;
    arr[1] = value + 1;
    arr[3] = value + 3;

    return arr[0] + arr[1] + arr[3]; // value * 3 + 4
}

noInline(testConditionalArrayUsage);
noInline(testNestedConditionalArray);
noInline(testEarlyReturnArray);

// Run tests to trigger optimization
for (let i = 0; i < testLoopCount; i++) {
    // Test the branch that uses the array
    let result1 = testConditionalArrayUsage(true, i % 100);
    let expected1 = (i % 100) + (i % 100 + 10) + (i % 100 + 20);
    assert(result1 === expected1, `testConditionalArrayUsage(true, ${i % 100}) failed: got ${result1}, expected ${expected1}`);

    // Test the branch that doesn't use the array
    let result2 = testConditionalArrayUsage(false, i % 100);
    let expected2 = (i % 100) * 3 + 30;
    assert(result2 === expected2, `testConditionalArrayUsage(false, ${i % 100}) failed: got ${result2}, expected ${expected2}`);

    // Test nested conditionals
    let val = i % 10;

    // outer=true, inner=true
    let result3 = testNestedConditionalArray(true, true, val);
    assert(result3 === val * 6, `testNestedConditionalArray(true, true, ${val}) failed: got ${result3}, expected ${val * 6}`);

    // outer=true, inner=false
    let result4 = testNestedConditionalArray(true, false, val);
    assert(result4 === val, `testNestedConditionalArray(true, false, ${val}) failed: got ${result4}, expected ${val}`);

    // outer=false, inner=true
    let result5 = testNestedConditionalArray(false, true, val);
    assert(result5 === val * 6, `testNestedConditionalArray(false, true, ${val}) failed: got ${result5}, expected ${val * 6}`);

    // outer=false, inner=false
    let result6 = testNestedConditionalArray(false, false, val);
    assert(result6 === val, `testNestedConditionalArray(false, false, ${val}) failed: got ${result6}, expected ${val}`);

    // Test early return
    let result7 = testEarlyReturnArray(true, val);
    assert(result7 === val * 42, `testEarlyReturnArray(true, ${val}) failed: got ${result7}, expected ${val * 42}`);

    let result8 = testEarlyReturnArray(false, val);
    let expected8 = val * 3 + 4;
    assert(result8 === expected8, `testEarlyReturnArray(false, ${val}) failed: got ${result8}, expected ${expected8}`);
}
