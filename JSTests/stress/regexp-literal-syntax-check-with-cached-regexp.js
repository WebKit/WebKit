function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrowSyntaxError(source) {
    let threw = false;
    try {
        eval(source);
    } catch (error) {
        threw = error instanceof SyntaxError;
    }
    shouldBe(threw, true);
}

new RegExp("a{");
new RegExp("\\k<b>");
new RegExp("x", "g");

for (let i = 0; i < 10; ++i) {
    shouldThrowSyntaxError("/a{/u");
    shouldThrowSyntaxError("/\\k<b>/u");
    shouldThrowSyntaxError("/x/gg");
    shouldThrowSyntaxError("/(/");
    shouldThrowSyntaxError("/a)/; 1");
    shouldBe(eval("/a{/").test("a{"), true);
    shouldBe(eval("/\\k<b>/").test("k<b>"), true);
    shouldBe(eval("/x/g").global, true);
    shouldBe(eval("/(?<n>x)\\k<n>/u").test("xx"), true);
}
