function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement) {
    return array.lastIndexOf(searchElement);
}
noInline(test);

// Test with duplicate elements - lastIndexOf should return the last occurrence
var array = [1, 2, 3, 4, 5, 3, 4, 5];
for (let i = 0; i < testLoopCount; i++) {
    sameValue(test(array, 3), 5);  // 3 appears at index 2 and 5, should return 5
    sameValue(test(array, 4), 6);  // 4 appears at index 3 and 6, should return 6
    sameValue(test(array, 5), 7);  // 5 appears at index 4 and 7, should return 7
    sameValue(test(array, 1), 0);  // 1 appears only at index 0
    sameValue(test(array, 32), -1); // 32 not found
}
