// Test basic RegExp.prototype[Symbol.replace] functionality

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Test 1: Basic non-global replace
shouldBe(/o/[Symbol.replace]("hello world", "0"), "hell0 world");
shouldBe(/l/[Symbol.replace]("hello world", "L"), "heLlo world");

// Test 2: Global replace
shouldBe(/o/g[Symbol.replace]("hello world", "0"), "hell0 w0rld");
shouldBe(/l/g[Symbol.replace]("hello world", "L"), "heLLo worLd");

// Test 3: No match
shouldBe(/xyz/[Symbol.replace]("hello world", "replaced"), "hello world");
shouldBe(/xyz/g[Symbol.replace]("hello world", "replaced"), "hello world");

// Test 4: Match at start
shouldBe(/hello/[Symbol.replace]("hello world", "hi"), "hi world");

// Test 5: Match at end
shouldBe(/world/[Symbol.replace]("hello world", "universe"), "hello universe");

// Test 6: Match entire string
shouldBe(/hello/[Symbol.replace]("hello", "hi"), "hi");
shouldBe(/hello/g[Symbol.replace]("hello", "hi"), "hi");

// Test 7: Empty replacement
shouldBe(/o/[Symbol.replace]("hello world", ""), "hell world");
shouldBe(/o/g[Symbol.replace]("hello world", ""), "hell wrld");

// Test 8: Replace with longer string
shouldBe(/b/[Symbol.replace]("abc", "BBB"), "aBBBc");

// Test 9: Case insensitive
shouldBe(/hello/i[Symbol.replace]("Hello World", "hi"), "hi World");
shouldBe(/hello/gi[Symbol.replace]("Hello World HELLO", "hi"), "hi World hi");

// Test 10: Multiline
shouldBe(/^world/m[Symbol.replace]("hello\nworld", "universe"), "hello\nuniverse");

// Test 11: Dot matches
shouldBe(/./[Symbol.replace]("hello", "X"), "Xello");
shouldBe(/./g[Symbol.replace]("hello", "X"), "XXXXX");

// Test 12: Word boundaries
shouldBe(/\bworld\b/[Symbol.replace]("hello world", "universe"), "hello universe");

// Test 13: Digit matching
shouldBe(/\d+/[Symbol.replace]("abc123def", "XXX"), "abcXXXdef");
shouldBe(/\d+/g[Symbol.replace]("abc123def456", "XXX"), "abcXXXdefXXX");

// Test 14: Repeated application (JIT warmup)
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/o/g[Symbol.replace]("hello world", "0"), "hell0 w0rld");
}

// Test 15: Replace via call
shouldBe(RegExp.prototype[Symbol.replace].call(/o/, "hello", "0"), "hell0");
shouldBe(RegExp.prototype[Symbol.replace].call(/o/g, "hello", "0"), "hell0");
