function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

// Default filler (space), small result -> flat path.
shouldBe("abc".padEnd(8), "abc     ");
shouldBe("abc".padStart(8), "     abc");

// Single-char filler.
shouldBe("abc".padEnd(8, "x"), "abcxxxxx");
shouldBe("abc".padStart(8, "x"), "xxxxxabc");

// Multi-char filler with truncation.
shouldBe("abc".padEnd(10, "123"), "abc1231231");
shouldBe("abc".padStart(10, "123"), "1231231abc");

// 16-bit this.
shouldBe("\u3042".padEnd(4, "x"), "\u3042xxx");
shouldBe("\u3042".padStart(4, "x"), "xxx\u3042");

// 16-bit filler.
shouldBe("a".padEnd(4, "\u3042"), "a\u3042\u3042\u3042");
shouldBe("a".padStart(4, "\u3042"), "\u3042\u3042\u3042a");

// Result length exactly at flat threshold (1024).
{
    let s = "abc".padEnd(1024);
    shouldBe(s.length, 1024);
    shouldBe(s.slice(0, 3), "abc");
    shouldBe(s[1023], " ");
}
{
    let s = "abc".padStart(1024);
    shouldBe(s.length, 1024);
    shouldBe(s.slice(1021), "abc");
    shouldBe(s[0], " ");
}

// Result length just above flat threshold (1025) -> rope path, must still be correct.
{
    let s = "abc".padEnd(1025, "xy");
    shouldBe(s.length, 1025);
    shouldBe(s.slice(0, 3), "abc");
    shouldBe(s.slice(3, 7), "xyxy");
    shouldBe(s[1024], "y");
}
{
    let s = "abc".padStart(1025, "xy");
    shouldBe(s.length, 1025);
    shouldBe(s.slice(1022), "abc");
    shouldBe(s.slice(0, 4), "xyxy");
}

// No padding needed.
shouldBe("hello".padEnd(3), "hello");
shouldBe("hello".padStart(3), "hello");

// Empty filler.
shouldBe("abc".padEnd(10, ""), "abc");
shouldBe("abc".padStart(10, ""), "abc");
