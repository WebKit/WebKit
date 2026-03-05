// Test RegExp.prototype[Symbol.replace] with named capture groups

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Test 1: Basic named capture group
shouldBe(/(?<word>\w+)/[Symbol.replace]("hello world", "[$<word>]"), "[hello] world");
shouldBe(/(?<word>\w+)/g[Symbol.replace]("hello world", "[$<word>]"), "[hello] [world]");

// Test 2: Multiple named capture groups
shouldBe(/(?<first>\w+) (?<second>\w+)/[Symbol.replace]("hello world", "$<second> $<first>"), "world hello");

// Test 3: Mixed numbered and named captures
shouldBe(/(?<first>\w+) (\w+)/[Symbol.replace]("hello world", "$<first>-$2"), "hello-world");

// Test 4: Named group that doesn't exist - becomes empty (group property is undefined)
shouldBe(/(?<word>\w+)/[Symbol.replace]("hello", "$<nonexistent>"), "");

// Test 5: Named group with special characters in name
shouldBe(/(?<word123>\w+)/[Symbol.replace]("hello", "[$<word123>]"), "[hello]");

// Test 6: Non-participating named capture group
shouldBe(/(?<a>a)(?<b>b)?(?<c>c)/[Symbol.replace]("ac", "$<a>-$<b>-$<c>"), "a--c");

// Test 7: Empty named group name (property "" doesn't exist, becomes empty)
shouldBe(/(?<word>\w+)/[Symbol.replace]("hello", "$<>"), "");

// Test 8: Unclosed named group reference (remains literal)
shouldBe(/(?<word>\w+)/[Symbol.replace]("hello", "$<word"), "$<word");

// Test 9: Named groups with functional replacement
let result = /(?<first>\w+) (?<second>\w+)/[Symbol.replace]("hello world", function(match, p1, p2, offset, string, groups) {
    return groups.second.toUpperCase() + " " + groups.first.toUpperCase();
});
shouldBe(result, "WORLD HELLO");

// Test 10: Named groups in functional replacement - verify groups object
let capturedGroups = null;
/(?<first>\w+) (?<second>\w+)/[Symbol.replace]("hello world", function(match, p1, p2, offset, string, groups) {
    capturedGroups = groups;
    return match;
});
shouldBe(capturedGroups.first, "hello");
shouldBe(capturedGroups.second, "world");

// Test 11: Combining $<name> with other substitutions
shouldBe(/(?<word>\w+)/[Symbol.replace]("hello world", "$<word>$&$<word>"), "hellohellohello world");

// Test 12: JIT warmup with named captures
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/(?<first>\w+) (?<second>\w+)/[Symbol.replace]("hello world", "$<second> $<first>"), "world hello");
}
