// Regression test for interpreter bug with nested greedy quantifiers and content backtracking.
// https://bugs.webkit.org/show_bug.cgi?id=307145
//
// The bug occurred in the interpreter when a greedy quantified parentheses dropped below
// its minimum count during backtracking. Instead of trying content backtracking within
// earlier iterations (to redistribute characters), it would immediately fail.
//
// @runDefault
// @runNoJIT

function test(pattern, input, expected) {
    let result = pattern.exec(input);
    let got = result ? result[0] : null;
    if (got !== expected)
        throw new Error("FAIL: pattern " + pattern + " on '" + input + "': expected '" + expected + "', got '" + got + "'");
}

// These patterns require content backtracking within nested quantifiers.
// The inner greedy a+ initially consumes too many characters, preventing the
// outer quantifier from reaching its minimum. Backtracking must redistribute
// characters across iterations.

// Inner min=2, variable count - the original failing case
test(/((a+){2,3}){2,3}$/, "aaaaaa", "aaaaaa");

// Similar patterns with different counts
test(/((a+){2,4}){2,3}$/, "aaaaaa", "aaaaaa");
test(/((a+){2,3}){2,4}$/, "aaaaaa", "aaaaaa");

// Non-capturing inner group
test(/((?:a+){2,3}){2,3}$/, "aaaaaa", "aaaaaa");

// Fixed outer, variable inner
test(/((a+){2,3}){2,2}$/, "aaaaaa", "aaaaaa");

// Three levels of nesting
test(/(((a+){2}){2}){1,2}$/, "aaaa", "aaaa");

// Ensure normal cases still work
test(/((a+){1,3}){2,3}$/, "aaaaaa", "aaaaaa");  // inner min=1 (simpler case)
test(/((a){2,3}){2,3}$/, "aaaaaa", "aaaaaa");   // non-greedy inner content

// Test no-match cases still correctly fail
test(/((a+){2,3}){2,3}$/, "aaa", null);  // not enough characters
