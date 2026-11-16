function shouldBe(actual, expected) {
    if (!Object.is(actual, expected)) {
        throw new Error(`Bad value: ${actual}!`);
    }
}

for (var i = 0; i < testLoopCount; i++) {
    // basic
    const str = "ABCDE";
    shouldBe(str.substr(0, 5), str);
    shouldBe(str.substr(0, 3), "ABC");
    shouldBe(str.substr(1, 3), "BCD");
    shouldBe(str.substr(2), "CDE");
    shouldBe(str.substr(2, 1), "C");
    shouldBe(str.substr(0), str);

    // length clamping
    shouldBe(str.substr(2, 0), "");
    shouldBe(str.substr(2, 100), "CDE");
    shouldBe(str.substr(0, 100), str);

    // negative length
    shouldBe(str.substr(2, -1), "");
    shouldBe(str.substr(0, -100), "");
    shouldBe(str.substr(-3, -1), "");

    // negative start
    shouldBe(str.substr(-2), "DE");
    shouldBe(str.substr(-2, 1), "D");
    shouldBe(str.substr(-2, 100), "DE");
    shouldBe(str.substr(-str.length), str);
    shouldBe(str.substr(-str.length - 1), str);
    shouldBe(str.substr(-100, 3), "ABC");
    shouldBe(str.substr(-100, 100), str);

    // out of range start
    shouldBe(str.substr(5), "");
    shouldBe(str.substr(5, 1), "");
    shouldBe(str.substr(100), "");
    shouldBe(str.substr(100, 100), "");

    // single-character fast path
    shouldBe(str.substr(0, 1), "A");
    shouldBe(str.substr(4, 1), "E");
    shouldBe(str.substr(-1, 1), "E");
    shouldBe(str.substr(-5, 1), "A");

    // NaN, Infinity
    shouldBe(str.substr(NaN), str);
    shouldBe(str.substr(NaN, NaN), "");
    shouldBe(str.substr(NaN, 2), "AB");
    shouldBe(str.substr(2, NaN), "");
    shouldBe(str.substr(Infinity), "");
    shouldBe(str.substr(-Infinity), str);
    shouldBe(str.substr(0, Infinity), str);
    shouldBe(str.substr(2, Infinity), "CDE");
    shouldBe(str.substr(Infinity, Infinity), "");
    shouldBe(str.substr(-Infinity, 2), "AB");
    shouldBe(str.substr(2, -Infinity), "");
    shouldBe(str.substr(-Infinity, -Infinity), "");
    shouldBe(str.substr(-Infinity, Infinity), str);
    shouldBe(str.substr(Infinity, -Infinity), "");

    // type cast
    shouldBe(str.substr("1", "3"), "BCD");
    shouldBe(str.substr("a", "c"), "");
    shouldBe(str.substr(true, false), "");
    shouldBe(str.substr(true, true), "B");
    shouldBe(str.substr(null, undefined), str);
    shouldBe(str.substr(undefined, null), "");
    shouldBe(str.substr("2", 2), "CD");
    shouldBe(str.substr(2, "2"), "CD");

    // decimal number
    shouldBe(str.substr(1.9, 2.2), "BC");
    shouldBe(str.substr(0.1, 0.9), "");
    shouldBe(str.substr(0.9, 1.1), "A");

    // empty string
    shouldBe("".substr(-10), "");
    shouldBe("".substr(0), "");
    shouldBe("".substr(10), "");
    shouldBe("".substr(0, 1), "");
    shouldBe("".substr(1, 1), "");
    shouldBe("".substr(-1, 2), "");

    // unicode
    const uni = "𠮷野家";
    shouldBe(uni.substr(0, 2), "𠮷");
    shouldBe(uni.substr(2, 99), "野家");
    shouldBe(uni.substr(-10, 10), "𠮷野家");

    // emoji
    const emoji = "🐟💨🍅🌈";
    shouldBe(emoji.substr(0, 2), "🐟");
    shouldBe(emoji.substr(2, 2), "💨");
    shouldBe(emoji.substr(0, 4), "🐟💨");
    shouldBe(emoji.substr(0, 0), "");
    shouldBe(emoji.substr(0), emoji);
    shouldBe(emoji.substr(-Infinity, Infinity), emoji);

    // edge cases
    shouldBe(str.substr(), str);
    shouldBe(str.substr(undefined), str);
    shouldBe(str.substr(null), str);
    shouldBe(str.substr(undefined, undefined), str);
    shouldBe(str.substr(0, 0), "");
    shouldBe(str.substr(0, -0), "");
    shouldBe(str.substr(-0, 0), "");
    shouldBe(str.substr(-0, -0), "");

    // rope strings (force ConcatString path)
    const rope = "foo" + (i & 1 ? "bar" : "baz");
    shouldBe(rope.substr(0, 3), "foo");
    shouldBe(rope.substr(3, 3), i & 1 ? "bar" : "baz");
    shouldBe(rope.substr(2, 1), "o");
    shouldBe(rope.substr(-3), i & 1 ? "bar" : "baz");
    shouldBe(rope.substr(2, 100), i & 1 ? "obar" : "obaz");
    shouldBe(rope.substr(100), "");
    shouldBe(rope.substr(2, 0), "");
    shouldBe(rope.substr(2, -1), "");
}
