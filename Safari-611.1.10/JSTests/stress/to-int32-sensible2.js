//@ runFTLNoCJIT

var testCases = [
    { value: -Number.MAX_VALUE, expected: 0 },
    { value: Number.MAX_VALUE, expected: 0 },
    { value: -Number.MIN_VALUE, expected: 0 },
    { value: Number.MIN_VALUE, expected: 0 },
    { value: -Infinity, expected: 0 },
    { value: Infinity, expected: 0 },
    { value: NaN, expected: 0 },
    { value: -NaN, expected: 0 },
    { value: 0, expected: 0 },
    { value: -0, expected: 0 },
    { value: 1, expected: 1 },
    { value: -1, expected: -1 },
    { value: 5, expected: 5 },
    { value: -5, expected: -5 },

    { value: 0x80000001, expected: -2147483647 },
    { value: 0x80000000, expected: -2147483648 },
    { value: 0x7fffffff, expected: 2147483647 },
    { value: 0x7ffffffe, expected: 2147483646 },

    { value: -2147483647, expected: -2147483647 },
    { value: -2147483648, expected: -2147483648 },
    { value: -2147483649, expected: 2147483647 },
    { value: -2147483650, expected: 2147483646 },
    
    { value: 2147483646, expected: 2147483646 },
    { value: 2147483647, expected: 2147483647 },
    { value: 2147483648, expected: -2147483648 },
    { value: 2147483649, expected: -2147483647 },
];

var numFailures = 0;
for (var i = 0; i < testCases.length; i++) {
    try {
        var testCase = testCases[i];
        var test = new Function("x", "y", "y = " + i + "; return x|0;");
        noInline(test);

        for (var k = 0; k < 10000; ++k) {
            actual = test(testCase.value);
            if (actual != testCase.expected)
                throw ("FAILED: x|0 where x = " + testCase.value + ": expected " + testCase.expected + ", actual " + actual);
        }
    } catch (e) {
        print(e);
        numFailures++;
    }
}

if (numFailures)
    throw("Found " + numFailures + " failures");

