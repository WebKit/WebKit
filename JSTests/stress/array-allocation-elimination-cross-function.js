function assert(condition, message) {
    if (!condition)
        throw new Error(message || "Assertion failed");
}

// Function that allocates and returns an array
function allocateArray(size, fillValue) {
    const arr = new Array(size);
    for (let i = 0; i < size; i++) {
        arr[i] = fillValue + i;
    }
    return arr;
}

// Function that processes an array but doesn't escape it
function processArray(arr) {
    let sum = 0;
    for (let i = 0; i < arr.length; i++) {
        sum += arr[i] * 2;
    }
    return sum;
}

// Function that modifies an array in place
function modifyArray(arr, multiplier) {
    for (let i = 0; i < arr.length; i++) {
        arr[i] *= multiplier;
    }
}

// Function that escapes the array by storing it globally
let globalArray = null;
function escapeArray(arr) {
    globalArray = arr;
    return arr.length;
}
noInline(escapeArray);

// Simple allocation + usage pattern
function allocateAndUse(size, value) {
    const arr = allocateArray(size, value);
    return processArray(arr);
}

// Allocation + modification + usage pattern
function allocateModifyAndUse(size, value) {
    const arr = allocateArray(size, value);
    modifyArray(arr, 3);
    return processArray(arr);
}

// Pattern where array sometimes escapes
function conditionalEscape(shouldEscape, size, value) {
    const arr = allocateArray(size, value);

    if (shouldEscape) {
        return escapeArray(arr);
    } else {
        return processArray(arr);
    }
}

// Chain of functions passing array
function chainedProcessing(size, value) {
    function step1(arr) {
        // Double all values
        for (let i = 0; i < arr.length; i++) {
            arr[i] *= 2;
        }
        return arr;
    }

    function step2(arr) {
        // Add index to each value
        for (let i = 0; i < arr.length; i++) {
            arr[i] += i;
        }
        return arr;
    }

    function step3(arr) {
        // Sum all values
        let sum = 0;
        for (let i = 0; i < arr.length; i++) {
            sum += arr[i];
        }
        return sum;
    }

    const initial = allocateArray(size, value);
    const after1 = step1(initial);
    const after2 = step2(after1);
    return step3(after2);
}

// Array allocated in inner function, used in outer
function nestedAllocation(size, value) {
    function innerAllocate() {
        return new Array(size).fill(value);
    }

    const arr = innerAllocate();
    let product = 1;
    for (let i = 0; i < arr.length; i++) {
        product *= arr[i];
    }
    return product;
}

noInline(allocateAndUse);
noInline(allocateModifyAndUse);
noInline(conditionalEscape);
noInline(chainedProcessing);
noInline(nestedAllocation);

// Test the cross-function scenarios
for (let i = 0; i < testLoopCount; i++) {
    const size = 3 + (i % 5); // sizes 3-7
    const value = i % 10;

    // Test simple allocation and usage
    let result1 = allocateAndUse(size, value);
    let expected1 = 0;
    for (let j = 0; j < size; j++) {
        expected1 += (value + j) * 2;
    }
    assert(result1 === expected1, `allocateAndUse(${size}, ${value}) failed: got ${result1}, expected ${expected1}`);

    // Test allocation, modification, and usage
    let result2 = allocateModifyAndUse(size, value);
    let expected2 = 0;
    for (let j = 0; j < size; j++) {
        expected2 += (value + j) * 3 * 2; // modified by 3, then doubled in processArray
    }
    assert(result2 === expected2, `allocateModifyAndUse(${size}, ${value}) failed: got ${result2}, expected ${expected2}`);

    // Test conditional escape - non-escaping case
    let result3 = conditionalEscape(false, size, value);
    let expected3 = expected1; // same as allocateAndUse
    assert(result3 === expected3, `conditionalEscape(false, ${size}, ${value}) failed: got ${result3}, expected ${expected3}`);

    // Test conditional escape - escaping case
    globalArray = null;
    let result4 = conditionalEscape(true, size, value);
    assert(result4 === size, `conditionalEscape(true, ${size}, ${value}) failed: got ${result4}, expected ${size}`);
    assert(globalArray !== null && globalArray.length === size, "Array was not properly escaped");

    // Test chained processing
    let result5 = chainedProcessing(size, value);
    let expected5 = 0;
    for (let j = 0; j < size; j++) {
        // step1: (value + j) * 2
        // step2: (value + j) * 2 + j
        // step3: sum all
        expected5 += (value + j) * 2 + j;
    }
    assert(result5 === expected5, `chainedProcessing(${size}, ${value}) failed: got ${result5}, expected ${expected5}`);

    // Test nested allocation (only when value > 0 to avoid zero product)
    if (value > 0) {
        let result6 = nestedAllocation(size, value);
        let expected6 = Math.pow(value, size);
        assert(result6 === expected6, `nestedAllocation(${size}, ${value}) failed: got ${result6}, expected ${expected6}`);
    }
}
