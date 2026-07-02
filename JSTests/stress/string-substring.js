function shouldBe(actual, expected) {
    if (!Object.is(actual, expected)) {
        throw new Error(`Bad value: ${actual}!`);
    }
}

for (var i = 0; i < testLoopCount; i++) {
    // basic
    const str = "ABCDE";
    shouldBe(str.substring(0, 5), str);
    shouldBe(str.substring(1, 4), "BCD");
    shouldBe(str.substring(2), "CDE");
    shouldBe(str.substring(3, 3), "");
    shouldBe(str.substring(4, 1), "BCD");

    // out of range
    shouldBe(str.substring(10, 12), "");
    shouldBe(str.substring(2, 10), "CDE");
    shouldBe(str.substring(99, 100), "");
    shouldBe(str.substring(5, 5), "");
    shouldBe(str.substring(5, 99), "");

    // negative value
    shouldBe(str.substring(-3), str);
    shouldBe(str.substring(-1), str);
    shouldBe(str.substring(-str.length), str);
    shouldBe(str.substring(-str.length - 1), str);
    shouldBe(str.substring(-3, 2), "AB");
    shouldBe(str.substring(1, -2), "A");
    shouldBe(str.substring(-5, -1), "");
    shouldBe(str.substring(-1, 3), "ABC");
    shouldBe(str.substring(0, -5), "");

    // NaN, Infinity
    shouldBe(str.substring(NaN), str);
    shouldBe(str.substring(NaN, NaN), "");
    shouldBe(str.substring(NaN, 2), "AB");
    shouldBe(str.substring(2, NaN), "AB");
    shouldBe(str.substring(Infinity), "");
    shouldBe(str.substring(-Infinity), str);
    shouldBe(str.substring(Infinity, 0), str);
    shouldBe(str.substring(0, Infinity), str);
    shouldBe(str.substring(Infinity, Infinity), "");
    shouldBe(str.substring(-Infinity, 2), "AB");
    shouldBe(str.substring(2, -Infinity), "AB");
    shouldBe(str.substring(-Infinity, -Infinity), "");
    shouldBe(str.substring(-Infinity, 0), "");
    shouldBe(str.substring(-Infinity, Infinity), str);
    shouldBe(str.substring(Infinity, -Infinity), str);

    // type cast
    shouldBe(str.substring("1", "3"), "BC");
    shouldBe(str.substring("3", "1"), "BC");
    shouldBe(str.substring("a", "c"), "");
    shouldBe(str.substring("A", "C"), "");
    shouldBe(str.substring(true, false), "A");
    shouldBe(str.substring(null, undefined), str);
    shouldBe(str.substring(undefined, null), "");
    shouldBe(str.substring(false, true), "A");
    shouldBe(str.substring("2", 4), "CD");
    shouldBe(str.substring(4, "2"), "CD");

    // decimal number
    shouldBe(str.substring(1.9, 4.2), "BCD");
    shouldBe(str.substring(0.1, 0.9), "");
    shouldBe(str.substring(0.9, 1.1), "A");

    // empty string
    shouldBe("".substring(-10), "");
    shouldBe("".substring(0), "");
    shouldBe("".substring(10), "");
    shouldBe("".substring(0, 1), "");
    shouldBe("".substring(1, 1), "");
    shouldBe("".substring(-1, 2), "");

    // unicode
    const uni = "𠮷野家";
    shouldBe(uni.substring(0, 2), "𠮷");
    shouldBe(uni.substring(0, 99), "𠮷野家");
    shouldBe(uni.substring(-10, 10), "𠮷野家");

    // emmoji
    const emoji1 = "🐟💨🍅🌈";
    shouldBe(emoji1.substring(0, 2), "🐟");
    shouldBe(emoji1.substring(2, 4), "💨");
    shouldBe(emoji1.substring(0, 4), "🐟💨");
    shouldBe(emoji1.substring(0, 0), "");
    shouldBe(emoji1.substring(0), emoji1);
    shouldBe(emoji1.substring(0, emoji1.length), emoji1);
    shouldBe(emoji1.substring(-Infinity, Infinity), emoji1);

    // edge cases
    shouldBe(str.substring(), str);
    shouldBe(str.substring(undefined), str);
    shouldBe(str.substring(null), str);
    shouldBe(str.substring(undefined, undefined), str);
    shouldBe(str.substring(0, 0), "");
    shouldBe(str.substring(0, -0), "");
    shouldBe(str.substring(-0, 0), "");
}
