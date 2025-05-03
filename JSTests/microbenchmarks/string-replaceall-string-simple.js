function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(str, foo, bar, baz) {
    return str.replaceAll("{{foo}}", foo)
                   .replaceAll("{{bar}}", bar)
                   .replaceAll("{{baz}}", baz);
}
noInline(test);

for (let i = 0; i < 1e6; i++) {
    shouldBe(test("{{foo}} and {{bar}} and {{baz}}", "AAA", "BBB", "CCC"), "AAA and BBB and CCC");
}
