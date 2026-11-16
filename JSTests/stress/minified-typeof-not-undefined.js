function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(x) {
    return typeof x < "u";
}
noInline(test);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(test(undefined), false);
    shouldBe(test(), false);

    shouldBe(test(null), true);
    shouldBe(test(1), true);
    shouldBe(test("foo"), true);
    shouldBe(test({}), true);
    shouldBe(test(function () {}), true);
    shouldBe(test(Symbol("test")), true);
    shouldBe(test(1n), true);
}
