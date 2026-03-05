// This test verifies that Boyer-Moore SIMD search correctly handles
// cases where beginIndex > 0. When the BM algorithm selects a range
// starting at a non-zero position (e.g., position 1 or 2), the SIMD
// path must read from the correct character position.
//
// The bug scenario:
// - Pattern like /[a-z][x].../ where position 0 has 26 candidates
// - BM algorithm selects beginIndex=1, endIndex=2 (single position 'x')
// - SIMD must check position 1 (the 'x'), not position 0

function testBoyerMooreSIMDBeginIndex() {
    // Test patterns that should trigger non-zero beginIndex selection
    // [a-z] has 26 candidates, so BM should skip it and select 'x' position

    // Pattern 1: beginIndex=1 case
    (function() {
        var re = /[a-z]x123456/;
        var tests = [
            { input: "m".repeat(50) + "bx123456" + "m".repeat(50), expected: true },
            { input: "m".repeat(50) + "ay123456" + "m".repeat(50), expected: false },
            { input: "bx123456", expected: true },
            { input: "ax12345", expected: false },
        ];
        for (var i = 0; i < tests.length; i++) {
            var result = re.test(tests[i].input);
            if (result !== tests[i].expected) {
                throw new Error("Pattern 1 test " + i + " failed: expected " + tests[i].expected + ", got " + result);
            }
        }
    })();

    // Pattern 2: Test near 16-byte boundaries (SIMD vector size)
    (function() {
        var re = /[0-9]xhello/;
        for (var offset = 0; offset < 32; offset++) {
            var shouldMatch = "a".repeat(offset) + "5xhello" + "a".repeat(50);
            var shouldNotMatch = "a".repeat(offset) + "5yhello" + "a".repeat(50);
            if (!re.test(shouldMatch)) {
                throw new Error("Pattern 2 should match at offset " + offset);
            }
            if (re.test(shouldNotMatch)) {
                throw new Error("Pattern 2 should not match at offset " + offset);
            }
        }
    })();

    // Pattern 3: beginIndex=2 case (two wide positions before narrow)
    (function() {
        var re = /[a-z][0-9]xworld/;
        var tests = [
            { input: "m".repeat(50) + "a5xworld" + "m".repeat(50), expected: true },
            { input: "m".repeat(50) + "b6yworld" + "m".repeat(50), expected: false },
        ];
        for (var i = 0; i < tests.length; i++) {
            var result = re.test(tests[i].input);
            if (result !== tests[i].expected) {
                throw new Error("Pattern 3 test " + i + " failed: expected " + tests[i].expected + ", got " + result);
            }
        }
    })();
}

testBoyerMooreSIMDBeginIndex();
