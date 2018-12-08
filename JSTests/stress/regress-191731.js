function assertEq(actual, expected) {
    if (actual != expected)
        throw ("Expected: " + expected + ", actual: " + actual);
}

function foo(arr, regexp, str) {
    regexp[Symbol.match](str);
    arr[1] = 3.54484805889626e-310;
    return arr[0];
}

let arr = [1.1, 2.2, 3.3];
let regexp = /a/y;

for (let i = 0; i < 10000; i++)
    foo(arr, regexp, "abcd");

regexp.lastIndex = {
    valueOf: () => {
        arr[0] = arr;
        return 0;
    }
};
let result = foo(arr, regexp, "abcd");

assertEq(arr[1], "3.54484805889626e-310");
assertEq(result, ",3.54484805889626e-310,3.3");
