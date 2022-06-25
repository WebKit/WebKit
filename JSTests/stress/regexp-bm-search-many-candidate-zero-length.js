function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var regexp = /a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z|0|1|2|3|4|5|6|7|8|9| |\t|\v|\n|\r|\$|\^|\&|\*|\(|\)/

for (var i = 0; i < 1e2; ++i) {
    shouldBe(regexp.test(`%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%`), false);
    shouldBe(regexp.test(`テスト`), false);
    shouldBe(regexp.test(`testing`), true);
    shouldBe(RegExp.leftContext, ``);
    shouldBe(RegExp.rightContext, `esting`);
}
