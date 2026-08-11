// Test that FixedCount patterns with empty alternatives and content backtracking work correctly.
// Regression test for zero-length match detection in FixedCount content backtracking.

// Pattern from slow.js that was hanging:
// (?:[^(?!)]||){23}z - FixedCount with 3 alternatives, 2 empty
if (/(?:[^(?!)]||){23}z/.test("/(?:[^(?!)]||){23}z/"))
    throw new Error("Should not match");

// FixedCount with empty alternatives - should allow zero-length iterations
if (!/(?:a||){5}z/.test("aaaz"))
    throw new Error("Should match: 3 iterations of 'a', 2 of empty, then 'z'");

// Content backtracking should still work for non-empty alternatives
var result = /(?:a|b){3}c/.exec("abbc");
if (!result || result[0] !== "abbc")
    throw new Error("Expected 'abbc', got " + result);

// FixedCount with all empty alternatives followed by required match
if (!/(?:||){10}z/.test("z"))
    throw new Error("Should match 'z' after 10 empty iterations");

// Another test: empty alternative should work
if (!/(?:a|){2}b/.test("b"))
    throw new Error("Should match: 2 empty iterations then 'b'");
