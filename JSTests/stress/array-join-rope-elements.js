function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function makeRope(a, b) {
    return a + b;
}
noInline(makeRope);

function test() {
    // 8-bit ropes with empty, 1-char, and multi-char separators.
    var arr = [makeRope("foo", "bar"), makeRope("baz", "qux"), "plain"];
    shouldBe(arr.join(''), "foobarbazquxplain");
    shouldBe(arr.join('&'), "foobar&bazqux&plain");
    shouldBe(arr.join('--'), "foobar--bazqux--plain");

    // 16-bit ropes.
    var arr16 = [makeRope("あい", "うえ"), makeRope("かき", "くけ")];
    shouldBe(arr16.join(''), "あいうえかきくけ");
    shouldBe(arr16.join('&'), "あいうえ&かきくけ");
    shouldBe(arr16.join('ー'), "あいうえーかきくけ");

    // Mixed 8-bit and 16-bit elements.
    shouldBe([makeRope("abc", "def"), makeRope("あ", "ん")].join('&'), "abcdef&あん");

    // Int32 elements interleaved with ropes.
    var mixed = [1, makeRope("a", "b"), 42, makeRope("c", "d"), -7];
    shouldBe(mixed.join(''), "1ab42cd-7");
    shouldBe(mixed.join('&'), "1&ab&42&cd&-7");

    // Substring ropes.
    var sub = makeRope("0123456789", "abcdefghij").substring(3, 15);
    shouldBe([sub, makeRope("X", "Y")].join('&'), "3456789abcde&XY");

    // Deep rope.
    var deep = "";
    for (var i = 0; i < 100; ++i)
        deep = makeRope(deep, "x" + i);
    shouldBe([deep, deep].join('|'), deep + "|" + deep);

    // Single element and empty strings.
    shouldBe([makeRope("solo", "!")].join('&'), "solo!");
    shouldBe(["", makeRope("", ""), ""].join('&'), "&&");
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test();
