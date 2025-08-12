function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error(`actual: ${actual}, expected: ${expected}`);
    }
}

for (var i = 0; i < testLoopCount; ++i) {
    var input = "abcdeabcdeabcdefghij";
    var re1 = new RegExp("abcde", "y");

    re1.test(input);
    shouldBe(re1.lastIndex, 5);

    var ret = input.replace(re1, "ABCDE");
    shouldBe(ret, "abcdeABCDEabcdefghij");
    shouldBe(re1.lastIndex, 10);

    ret = input.replace(re1, "ABCDE");
    shouldBe(ret, "abcdeabcdeABCDEfghij");
    shouldBe(re1.lastIndex, 15);

    ret = input.replace(re1, "ABCDE");
    shouldBe(ret, "abcdeabcdeabcdefghij");
    shouldBe(re1.lastIndex, 0);

    var re2 = /a/y;

    re2.lastIndex = -1;
    shouldBe("a".replace(re2, "b"), "b");
    // shouldBe(re2.lastIndex, 1);

    re2.lastIndex = 0;
    shouldBe("a".replace(re2, "b"), "b");
    // shouldBe(re2.lastIndex, 1);

    re2.lastIndex = 1;
    shouldBe("a".replace(re2, "b"), "a");
    shouldBe(re2.lastIndex, 0);

    re2.lastIndex = 2;
    shouldBe("a".replace(re2, "b"), "a");
    shouldBe(re2.lastIndex, 0);

    re2.lastIndex = Infinity;
    shouldBe("a".replace(re2, "b"), "a");
    shouldBe(re2.lastIndex, 0);

    re2.lastIndex = -Infinity;
    shouldBe("a".replace(re2, "b"), "b");
    shouldBe(re2.lastIndex, 1);

    re2.lastIndex = NaN;
    shouldBe("a".replace(re2, "b"), "b");
    shouldBe(re2.lastIndex, 1);

    re2.lastIndex = "foo";
    shouldBe("a".replace(re2, "b"), "b");
    shouldBe(re2.lastIndex, 1);

    re2.lastIndex = "1";
    shouldBe("a".replace(re2, "b"), "a");
    shouldBe(re2.lastIndex, 0);

    re2.lastIndex = {};
    shouldBe("a".replace(re2, "b"), "b");
    shouldBe(re2.lastIndex, 1);
}
