// Regression test for nested quantified parentheses crash.
// The bug occurred when:
// 1. An outer quantified parentheses group with multiple alternatives
// 2. One alternative contains a nested quantified parentheses group
// 3. Other alternatives don't contain the nested group
// 4. The nested group's frame slots (parenContextHead) were uninitialized
//    when the alternative containing it was entered after other alternatives failed.

// The pattern that triggered the crash:
// (a|b|c(){2}$){2}$ - outer group {2} with alternatives a, b, and c(){2}$
// The inner (){2} is only in the 'c' alternative.

var r = new RegExp('(a|b|c(){2}$){2}$');

function test(input, expected) {
    var result = input.match(r);
    if (expected === null) {
        if (result !== null)
            throw new Error("Expected null for input '" + input + "', got: " + JSON.stringify(result));
    } else {
        if (result === null)
            throw new Error("Expected match for input '" + input + "', got null");
        if (JSON.stringify(result) !== JSON.stringify(expected))
            throw new Error("Expected " + JSON.stringify(expected) + " for input '" + input + "', got: " + JSON.stringify(result));
    }
}

// Cases that match (alternative a or b taken, no nested group execution)
test("aa", ["aa", "a", null]);
test("ab", ["ab", "b", null]);
test("ba", ["ba", "a", null]);
test("bb", ["bb", "b", null]);

// Cases where alternative c is taken (nested group executes)
test("ac", ["ac", "c", ""]);
test("bc", ["bc", "c", ""]);

// The original crash case - enters 'c' alternative after 'a' and 'b' fail
test("abc", ["bc", "c", ""]);

// Cases that don't match
test("cc", null);
test("aabbbccc", null);
