function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement) {
    return array.lastIndexOf(searchElement);
}
noInline(test);

// Test with duplicate elements - lastIndexOf should return the last occurrence
var array = ["foo", "bar", "baz", "foo", "qux", "bar", "baz", "qux"];
for (let i = 0; i < testLoopCount; i++) {
    sameValue(test(array, "foo"), 3);  // "foo" appears at index 0 and 3, should return 3
    sameValue(test(array, "bar"), 5);  // "bar" appears at index 1 and 5, should return 5
    sameValue(test(array, "baz"), 6);  // "baz" appears at index 2 and 6, should return 6
    sameValue(test(array, "qux"), 7);  // "qux" appears at index 4 and 7, should return 7
    sameValue(test(array, "xxx"), -1); // "xxx" not found
}
