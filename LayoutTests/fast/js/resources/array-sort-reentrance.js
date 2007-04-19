description(
"This tests that a call to array.sort(compareFunction) does not crash from within a sort comparison function."
);

var numbers1 = [1, 2, 3, 4, 5, 6, 7];
var numbers2 = numbers1.slice();

function compareFn1(a, b) {
    return b - a;
}

function compareFn2(a, b) {
    numbers1.sort(compareFn1);
    return b - a;
}

numbers2.sort(compareFn2);

var successfullyParsed = true;
