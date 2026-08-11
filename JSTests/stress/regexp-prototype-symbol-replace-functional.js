// Test RegExp.prototype[Symbol.replace] with functional replacement

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Test 1: Basic functional replacement
shouldBe(/\w+/[Symbol.replace]("hello world", function(match) {
    return match.toUpperCase();
}), "HELLO world");

// Test 2: Global functional replacement
shouldBe(/\w+/g[Symbol.replace]("hello world", function(match) {
    return match.toUpperCase();
}), "HELLO WORLD");

// Test 3: Functional replacement with capture groups
shouldBe(/(\w)(\w+)/g[Symbol.replace]("hello world", function(match, first, rest) {
    return first.toUpperCase() + rest;
}), "Hello World");

// Test 4: Functional replacement receives offset
let offsets = [];
/\w+/g[Symbol.replace]("hello world", function(match, offset) {
    offsets.push(offset);
    return match;
});
shouldBe(offsets[0], 0);
shouldBe(offsets[1], 6);

// Test 5: Functional replacement receives full string
let receivedString = null;
/\w+/[Symbol.replace]("hello world", function(match, offset, string) {
    receivedString = string;
    return match;
});
shouldBe(receivedString, "hello world");

// Test 6: Functional replacement with capture groups - all arguments
let allArgs = null;
/(\w+) (\w+)/[Symbol.replace]("hello world", function(match, p1, p2, offset, string) {
    allArgs = { match, p1, p2, offset, string };
    return match;
});
shouldBe(allArgs.match, "hello world");
shouldBe(allArgs.p1, "hello");
shouldBe(allArgs.p2, "world");
shouldBe(allArgs.offset, 0);
shouldBe(allArgs.string, "hello world");

// Test 7: Functional replacement returns non-string (should be converted)
shouldBe(/b/[Symbol.replace]("abc", function() { return 123; }), "a123c");
shouldBe(/b/[Symbol.replace]("abc", function() { return null; }), "anullc");
shouldBe(/b/[Symbol.replace]("abc", function() { return undefined; }), "aundefinedc");
shouldBe(/b/[Symbol.replace]("abc", function() { return { toString() { return "X"; } }; }), "aXc");

// Test 8: Functional replacement with empty return
shouldBe(/b/[Symbol.replace]("abc", function() { return ""; }), "ac");

// Test 9: Functional replacement counter
let count = 0;
/a/g[Symbol.replace]("aaa", function() {
    count++;
    return "b";
});
shouldBe(count, 3);

// Test 10: Functional replacement with non-participating groups
let receivedP2 = "not set";
/(a)(b)?(c)/[Symbol.replace]("ac", function(match, p1, p2, p3, offset, string) {
    receivedP2 = p2;
    return match;
});
shouldBe(receivedP2, undefined);

// Test 11: Functional replacement that references other matches
let results = [];
/[a-z](\d)/g[Symbol.replace]("a1b2c3", function(match, digit) {
    results.push(digit);
    return "[" + digit + "]";
});
shouldBe(results.join(","), "1,2,3");

// Test 12: Functional replacement using closure
let multiplier = 2;
shouldBe(/\d/g[Symbol.replace]("1 2 3", function(match) {
    return String(parseInt(match) * multiplier);
}), "2 4 6");

// Test 13: Arrow function replacement
shouldBe(/./g[Symbol.replace]("hello", c => c.toUpperCase()), "HELLO");

// Test 14: JIT warmup with functional replacement
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/\w+/g[Symbol.replace]("hello world", m => m.toUpperCase()), "HELLO WORLD");
}

// Test 15: Functional replacement with many capture groups
shouldBe(/(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)/[Symbol.replace]("abcdefghij", function(match, ...groups) {
    // groups[0-9] are capture groups, groups[10] is offset, groups[11] is string
    return groups.slice(0, 10).reverse().join("");
}), "jihgfedcba");
