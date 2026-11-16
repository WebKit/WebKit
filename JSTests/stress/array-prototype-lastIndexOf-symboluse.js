function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement) {
    return array.lastIndexOf(searchElement);
}
noInline(test);

// Test with duplicate elements - lastIndexOf should return the last occurrence
const sym1 = Symbol("sym1");
const sym2 = Symbol("sym2");
const sym3 = Symbol("sym3");
const array = [sym1, sym2, sym3, sym1, 1, sym2, sym3, 2];
for (let i = 0; i < testLoopCount; i++) {
    sameValue(test(array, sym1), 3);  // sym1 appears at index 0 and 3, should return 3
    sameValue(test(array, sym2), 5);  // sym2 appears at index 1 and 5, should return 5
    sameValue(test(array, sym3), 6);  // sym3 appears at index 2 and 6, should return 6
    sameValue(test(array, Symbol("other")), -1); // different symbol not found
}
