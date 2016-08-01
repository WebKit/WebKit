//@ runDefault
// This test should not crash

function assertEqual(actual, expected) {
    function toString(x) {
        return '' + x;
    }
    if (typeof actual != typeof expected)
        throw Error("Failed: typeof expected: '" + typeof(expected) + "', typeof actual: '" + typeof(actual) + "'");;
    
    if (toString(actual) != toString(expected))
        throw Error("Failed: expected: '" + toString(expected) + "', actual: '" + toString(actual) + "'");;
}

assertEqual(Array.prototype.length, 0);

Array.prototype.length = 0x40000000;

assertEqual(Array.prototype.length, 0x40000000);
