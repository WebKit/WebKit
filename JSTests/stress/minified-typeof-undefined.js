function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(x) {
    return typeof x > "u";
}
noInline(test);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(test(undefined), true);
    shouldBe(test(), true);

    shouldBe(test(null), false);
    shouldBe(test(1), false);
    shouldBe(test("foo"), false);
    shouldBe(test({}), false);
    shouldBe(test(function () {}), false);
    shouldBe(test(Symbol("test")), false);
    shouldBe(test(1n), false);
}
