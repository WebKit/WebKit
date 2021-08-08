function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
var regexp = /script/;
for (var i = 0; i < 1e2; ++i) {
    shouldBe(/script/.test("testtestscript"), true);
    shouldBe(RegExp.leftContext, "testtest");
    shouldBe(/script/.test($vm.make16BitStringIfPossible("testtestscript")), true);
    shouldBe(RegExp.leftContext, "testtest");
}
