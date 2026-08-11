function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var regexp = /今日|明日/i;

for (var i = 0; i < 1e2; ++i) {
    shouldBe(regexp.test("昨日遅くに寝たので今日はまだ眠い。"), true);
    shouldBe(RegExp.leftContext, "昨日遅くに寝たので");
    shouldBe(RegExp.rightContext, "はまだ眠い。");
    shouldBe(regexp.test("Because I went to bed late last night, I'm still sleepy"), false);
}
