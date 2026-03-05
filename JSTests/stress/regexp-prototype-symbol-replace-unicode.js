// Test RegExp.prototype[Symbol.replace] with unicode mode

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Test 1: Basic unicode flag
shouldBe(/./gu[Symbol.replace]("\u{1F600}\u{1F601}", "X"), "XX");

// Test 2: Without unicode flag, surrogate pairs are treated as two characters
shouldBe(/./g[Symbol.replace]("\u{1F600}", "X"), "XX"); // Two code units
shouldBe(/./gu[Symbol.replace]("\u{1F600}", "X"), "X");  // One code point

// Test 3: Unicode property escapes
shouldBe(/\p{L}/gu[Symbol.replace]("aαβγ123", "X"), "XXXX123");
shouldBe(/\p{N}/gu[Symbol.replace]("aαβγ123", "X"), "aαβγXXX");

// Test 4: Unicode character classes
shouldBe(/\w/gu[Symbol.replace]("café", "X"), "XXXé"); // \w doesn't match é even in unicode mode

// Test 5: Empty match advancement in unicode mode
shouldBe(/(?:)/gu[Symbol.replace]("a\u{1F600}b", "-"), "-a-\u{1F600}-b-");

// Test 6: Unicode surrogate pairs with capture groups
let emoji = "\u{1F600}";
// Without u flag, . matches first surrogate only, so capture is just the first surrogate
let withoutU = /(.)/[Symbol.replace](emoji, "[$1]");
shouldBe(withoutU, "[\ud83d]\ude00");
// With u flag, . matches the entire code point
let withU = /(.)/u[Symbol.replace](emoji, "[$1]");
shouldBe(withU, "[" + emoji + "]");

// Test 7: Unicode case folding
shouldBe(/hello/iu[Symbol.replace]("HELLO", "world"), "world");
shouldBe(/ω/iu[Symbol.replace]("Ω", "omega"), "omega"); // Greek capital omega -> lowercase omega

// Test 8: Unicode line terminators
shouldBe(/./gu[Symbol.replace]("a\u2028b", "X"), "X\u2028X"); // Line separator not matched by .
shouldBe(/./gu[Symbol.replace]("a\u2029b", "X"), "X\u2029X"); // Paragraph separator not matched by .

// Test 9: Unicode with global and sticky
shouldBe(/./guy[Symbol.replace]("ab", "X"), "XX");

// Test 10: Unicode escape sequences in replacement
shouldBe(/b/[Symbol.replace]("abc", "\u0058"), "aXc"); // \u0058 is 'X'

// Test 11: Unicode in functional replacement
shouldBe(/./gu[Symbol.replace]("\u{1F600}\u{1F601}", function(match) {
    return match === "\u{1F600}" ? "A" : "B";
}), "AB");

// Test 12: Unicode with capture groups
shouldBe(/(\u{1F600})(.*)(\u{1F601})/u[Symbol.replace]("\u{1F600}x\u{1F601}", "$3$2$1"), "\u{1F601}x\u{1F600}");

// Test 13: Unicode with v flag (unicodeSets)
try {
    // unicodeSets flag (v) - may not be supported in all environments
    shouldBe(/[aeiou]/gv[Symbol.replace]("aeiou", "X"), "XXXXX");
} catch (e) {
    // Skip if v flag not supported
    if (!(e instanceof SyntaxError)) {
        throw e;
    }
}

// Test 14: JIT warmup with unicode
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/./gu[Symbol.replace]("\u{1F600}\u{1F601}", "X"), "XX");
}
