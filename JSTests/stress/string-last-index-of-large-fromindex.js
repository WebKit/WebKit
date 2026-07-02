// ECMA-262 reference implementation for cross-validation.
// https://tc39.es/ecma262/#sec-string.prototype.lastindexof
function specLastIndexOf(S, searchStr, position) {
    var numPos = +position; // ToNumber
    var pos;
    if (numPos !== numPos)
        pos = Infinity;
    else if (numPos === 0 || !isFinite(numPos))
        pos = numPos === -Infinity ? -Infinity : numPos;
    else
        pos = numPos < 0 ? Math.ceil(numPos) : Math.floor(numPos);
    var len = S.length;
    var searchLen = searchStr.length;
    if (len < searchLen)
        return -1;
    // clamp pos to [0, len - searchLen]
    var maxStart = len - searchLen;
    var start;
    if (pos < 0)
        start = 0;
    else if (pos > maxStart)
        start = maxStart;
    else
        start = pos | 0; // The issue was here for very large positive 'pos' values.
    // Scan backward.
    for (var i = start; i >= 0; i--) {
        var match = true;
        for (var j = 0; j < searchLen; j++) {
            if (S.charCodeAt(i + j) !== searchStr.charCodeAt(j)) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return -1;
}

function assertEq(a, b, m) {
    if (a !== b)
        throw new Error(m + ": expected " + b + " got " + a);
}

function check(S, needle, pos, message) {
    var expected = specLastIndexOf(S, needle, pos);
    var actual = pos === undefined ? S.lastIndexOf(needle) : S.lastIndexOf(needle, pos);
    if (actual !== expected)
        throw new Error(message + ": S.lastIndexOf(" + JSON.stringify(needle) + ", " + String(pos) + ") expected " + expected + " got " + actual);
}

const largeNumber = 2**32 + 100; // Larger than max unsigned int
const veryLargeNumber = 2**53 + 100; // Larger than MAX_SAFE_INTEGER

let testString = "abcabcabc"; // length 9
let needle = "abc"; // length 3

// Test cases for large fromIndex
check(testString, needle, testString.length, `large fromIndex: string length`);
check(testString, needle, testString.length + 100, `large fromIndex: string length + 100`);
check(testString, needle, Infinity, `large fromIndex: Infinity`);
check(testString, needle, largeNumber, `large fromIndex: 2^32 + 100`);
check(testString, needle, veryLargeNumber, `large fromIndex: 2^53 + 100`);

// Test cases where fromIndex is explicitly very large and would cause wrap around
// if not clamped properly.
let longString = "a".repeat(1000) + "b"; // length 1001
let shortNeedle = "b"; // length 1

check(longString, shortNeedle, longString.length + 100, `very long string, large fromIndex`);
check(longString, shortNeedle, largeNumber, `very long string, very large fromIndex`);
check(longString, shortNeedle, veryLargeNumber, `very long string, extremely large fromIndex`);

// Test with empty string and large fromIndex
check("", "", largeNumber, `empty string, large fromIndex`);
check("a", "", largeNumber, `single char string, empty needle, large fromIndex`);
