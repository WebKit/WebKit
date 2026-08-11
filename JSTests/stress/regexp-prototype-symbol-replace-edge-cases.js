// Test RegExp.prototype[Symbol.replace] edge cases

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try {
        func();
    } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${e.constructor.name}: ${e.message}`);
    }
    if (!threw)
        throw new Error(`Expected ${errorType.name} to be thrown`);
}

// Test 1: Empty string input
shouldBe(/a/[Symbol.replace]("", "b"), "");
shouldBe(/a/g[Symbol.replace]("", "b"), "");
shouldBe(/(?:)/[Symbol.replace]("", "X"), "X");
shouldBe(/(?:)/g[Symbol.replace]("", "X"), "X");

// Test 2: Empty match global replacement
shouldBe(/(?:)/g[Symbol.replace]("abc", "-"), "-a-b-c-");
shouldBe(/(?:)/g[Symbol.replace]("", "-"), "-");

// Test 3: Empty replacement with empty match
shouldBe(/(?:)/g[Symbol.replace]("abc", ""), "abc");

// Test 4: Match that returns empty string
shouldBe(/a*/g[Symbol.replace]("aaa", "X"), "XX"); // Matches "aaa" then "" at end

// Test 5: Very long replacement string
let longStr = "x".repeat(10000);
shouldBe(/a/[Symbol.replace]("a", longStr), longStr);

// Test 6: Very many matches
let manyAs = "a".repeat(1000);
shouldBe(/a/g[Symbol.replace](manyAs, "b"), "b".repeat(1000));

// Test 7: Replacement with special regex characters (should be literal)
shouldBe(/b/[Symbol.replace]("abc", "$"), "a$c");
shouldBe(/b/[Symbol.replace]("abc", "^"), "a^c");
shouldBe(/b/[Symbol.replace]("abc", "\\"), "a\\c");
shouldBe(/b/[Symbol.replace]("abc", "["), "a[c");
shouldBe(/b/[Symbol.replace]("abc", "("), "a(c");

// Test 8: Non-string replacement value conversion
shouldBe(/b/[Symbol.replace]("abc", 123), "a123c");
shouldBe(/b/[Symbol.replace]("abc", true), "atruec");
shouldBe(/b/[Symbol.replace]("abc", false), "afalsec");
shouldBe(/b/[Symbol.replace]("abc", null), "anullc");
shouldBe(/b/[Symbol.replace]("abc", undefined), "aundefinedc");
shouldBe(/b/[Symbol.replace]("abc", { toString() { return "X"; } }), "aXc");

// Test 9: Non-string input conversion (via call)
shouldBe(/2/[Symbol.replace](123, "X"), "1X3");
shouldBe(/r/[Symbol.replace](true, "X"), "tXue");

// Test 10: Replacement with $ at end
shouldBe(/b/[Symbol.replace]("abc", "X$"), "aX$c");
shouldBe(/b/[Symbol.replace]("abc", "$"), "a$c");

// Test 11: $< without closing >
shouldBe(/(?<x>b)/[Symbol.replace]("abc", "$<x"), "a$<xc");
shouldBe(/(?<x>b)/[Symbol.replace]("abc", "$<"), "a$<c");

// Test 12: Overlapping matches (non-overlapping by design)
shouldBe(/aa/g[Symbol.replace]("aaaa", "X"), "XX");

// Test 13: Backreference larger than group count
shouldBe(/(a)/[Symbol.replace]("abc", "$9"), "$9bc"); // Only 1 group, $9 is literal
shouldBe(/(a)/[Symbol.replace]("abc", "$99"), "$99bc");

// Test 14: lastIndex behavior with global flag
let re = /a/g;
re.lastIndex = 100;
re[Symbol.replace]("aaa", "X"); // lastIndex should be reset to 0 for global replace
// The result should still work correctly
shouldBe(/a/g[Symbol.replace]("aaa", "X"), "XXX");

// Test 15: lastIndex behavior with non-global flag
re = /a/;
re.lastIndex = 100;
shouldBe(re[Symbol.replace]("aaa", "X"), "Xaa"); // lastIndex ignored for non-global

// Test 16: Match at position 0
shouldBe(/^/[Symbol.replace]("abc", "X"), "Xabc");
shouldBe(/^/g[Symbol.replace]("abc", "X"), "Xabc");

// Test 17: Match at end position
shouldBe(/$/[Symbol.replace]("abc", "X"), "abcX");
shouldBe(/$/g[Symbol.replace]("abc", "X"), "abcX");

// Test 18: Type error when called on non-object
shouldThrow(() => {
    RegExp.prototype[Symbol.replace].call(null, "test", "x");
}, TypeError);

shouldThrow(() => {
    RegExp.prototype[Symbol.replace].call(undefined, "test", "x");
}, TypeError);

shouldThrow(() => {
    RegExp.prototype[Symbol.replace].call(123, "test", "x");
}, TypeError);

// Test 19: Works on plain objects with exec
let fakeRegExp = {
    flags: "",
    exec: function(str) {
        if (this.called) return null;
        this.called = true;
        let result = ["found"];
        result.index = 0;
        result.groups = undefined;
        return result;
    }
};
shouldBe(RegExp.prototype[Symbol.replace].call(fakeRegExp, "found it", "replaced"), "replaced it");

// Test 20: Works with sticky flag
shouldBe(/a/y[Symbol.replace]("aaab", "X"), "Xaab");
shouldBe(/a/gy[Symbol.replace]("aaab", "X"), "XXXb");

// Test 21: JIT warmup edge cases
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/(?:)/g[Symbol.replace]("", "X"), "X");
    shouldBe(/(?:)/g[Symbol.replace]("abc", "-"), "-a-b-c-");
}
