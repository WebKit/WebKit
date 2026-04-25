function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function t(s) {
    /h(el)lo/.exec(s);
}
noInline(t);

for (let i = 0; i < testLoopCount; i++) {
    /s(ee)d/.exec("seed");
    t("hello");
    shouldBe(RegExp.$1, "el");
    shouldBe(RegExp.lastMatch, "hello");
}
