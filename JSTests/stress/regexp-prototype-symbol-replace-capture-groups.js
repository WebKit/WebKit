// Test RegExp.prototype[Symbol.replace] capture groups and backreferences

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Test 1: Single capture group with $1
shouldBe(/(\w+)/[Symbol.replace]("hello world", "[$1]"), "[hello] world");
shouldBe(/(\w+)/g[Symbol.replace]("hello world", "[$1]"), "[hello] [world]");

// Test 2: Multiple capture groups
shouldBe(/(\w+) (\w+)/[Symbol.replace]("hello world", "$2 $1"), "world hello");
shouldBe(/(\w+) (\w+)/[Symbol.replace]("John Smith", "$2, $1"), "Smith, John");

// Test 3: Backreference with more than 9 groups
shouldBe(/(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)/[Symbol.replace]("abcdefghijk", "$11$10$9$8$7$6$5$4$3$2$1"), "kjihgfedcba");

// Test 4: Nested capture groups
shouldBe(/((a)(b)(c))/[Symbol.replace]("abc", "[$1][$2][$3][$4]"), "[abc][a][b][c]");

// Test 5: Non-participating capture groups (undefined captures)
shouldBe(/(a)(b)?(c)/[Symbol.replace]("ac", "$1-$2-$3"), "a--c");

// Test 6: $& - the matched substring
shouldBe(/l/[Symbol.replace]("hello", "[$&]"), "he[l]lo");
shouldBe(/l/g[Symbol.replace]("hello", "[$&]"), "he[l][l]o");
shouldBe(/ll/[Symbol.replace]("hello", "[$&]"), "he[ll]o");

// Test 7: $` - the portion before the match
shouldBe(/b/[Symbol.replace]("abc", "[$`]"), "a[a]c");
shouldBe(/world/[Symbol.replace]("hello world", "[$`]"), "hello [hello ]");

// Test 8: $' - the portion after the match
shouldBe(/b/[Symbol.replace]("abc", "[$']"), "a[c]c");
shouldBe(/hello/[Symbol.replace]("hello world", "[$']"), "[ world] world");

// Test 9: $$ - literal dollar sign
shouldBe(/l/[Symbol.replace]("hello", "$$"), "he$lo");
shouldBe(/l/g[Symbol.replace]("hello", "$$"), "he$$o");
shouldBe(/ll/[Symbol.replace]("hello", "$$$$"), "he$$o");

// Test 10: Invalid backreference (higher than number of groups)
shouldBe(/(a)/[Symbol.replace]("abc", "$2"), "$2bc"); // $2 is literal since only 1 group
shouldBe(/(a)(b)/[Symbol.replace]("abc", "$3"), "$3c"); // $3 is literal since only 2 groups

// Test 11: $0 is not a valid backreference
shouldBe(/(a)/[Symbol.replace]("abc", "$0"), "$0bc");
shouldBe(/(a)/[Symbol.replace]("abc", "$01"), "abc"); // $01 is $1

// Test 12: Mixed backreferences and literal text
shouldBe(/(h)(e)(l)(l)(o)/[Symbol.replace]("hello", "$1-$2-$3-$4-$5"), "h-e-l-l-o");

// Test 13: Backreference at different positions
shouldBe(/(abc)/[Symbol.replace]("abc", "$1$1$1"), "abcabcabc");
shouldBe(/(abc)/[Symbol.replace]("abc", "<<$1>>"), "<<abc>>");

// Test 14: Empty capture groups
shouldBe(/(a)()?(b)/[Symbol.replace]("ab", "$1$2$3"), "ab");

// Test 15: JIT warmup with capture groups
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/(\w+) (\w+)/[Symbol.replace]("hello world", "$2 $1"), "world hello");
}
