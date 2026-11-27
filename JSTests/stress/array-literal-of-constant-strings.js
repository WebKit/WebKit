function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function strings() {
    return ["alpha", "beta", "alpha", "gamma-delta", "", "\u3042", "a"];
}

function mixed() {
    return ["alpha", "beta", "alpha", 1, 2.5, null, true];
}

function sharedWithConstant(x) {
    const prefix = x + "alpha";
    return [prefix, ["alpha", "beta"], ["beta", 1]];
}

function withVariable(x) {
    return [["alpha", x], [x, "alpha"], ["alpha", ...x], ["alpha", , "beta"]];
}

for (let i = 0; i < testLoopCount; ++i) {
    const a = strings();
    shouldBe(a.length, 7);
    shouldBe(a.join(","), "alpha,beta,alpha,gamma-delta,,\u3042,a");
    shouldBe(a.indexOf("beta"), 1);
    shouldBe(a.indexOf("gamma-" + "delta"), 3);
    shouldBe(a.indexOf(""), 4);
    shouldBe(a.indexOf("\u3042"), 5);
    shouldBe(a.indexOf("a"), 6);
    shouldBe(a.indexOf("delta"), -1);
    shouldBe(a.includes("alpha"), true);
    shouldBe(a.includes("omega"), false);
    shouldBe(strings() !== a, true);

    const m = mixed();
    shouldBe(m.join(","), "alpha,beta,alpha,1,2.5,,true");
    shouldBe(m.indexOf("beta"), 1);

    const [prefix, sharedStrings, sharedMixed] = sharedWithConstant("x");
    shouldBe(prefix, "xalpha");
    shouldBe(sharedStrings.join(","), "alpha,beta");
    shouldBe(sharedStrings.indexOf("beta"), 1);
    shouldBe(sharedMixed.join(","), "beta,1");

    const [first, last, spread, holey] = withVariable("x");
    shouldBe(first.join(","), "alpha,x");
    shouldBe(last.join(","), "x,alpha");
    shouldBe(spread.join(","), "alpha,x");
    shouldBe(holey.length, 3);
    shouldBe(1 in holey, false);
    shouldBe(holey.join(","), "alpha,,beta");

    if (!(i % 1000))
        gc();
}
