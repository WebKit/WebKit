//@ memoryHog!

// This test should not crash

testCases = [
   // under the limits after describe(...)
   { "len": 2147483586, "shouldThrow": false },
    // for the following:
    // original string is fine, but result of describe(...) is too long
   { "len": 2147483587, "shouldThrow": true },
   { "len": 2147483588, "shouldThrow": true },
   { "len": 2147483647, "shouldThrow": true },
    // caught by bounds check in padEnd
   { "len": 2147483648, "shouldThrow": true },
];

for (const tc of testCases) {
    try {
        // debug("Trying to describe string of length " + tc['len']);
        const string = ' '.padEnd(tc['len']);
        describe(string);
    } catch (e) {
        if (!tc['shouldThrow']) {
            throw new Error(`Calling describe on a string of length ${tc['len']} threw an exception \'${e}\' when it shouldn't have.`);
        }
    }
}
