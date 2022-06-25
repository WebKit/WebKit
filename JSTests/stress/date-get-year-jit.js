function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(date) {
    return date.getYear();
}
noInline(test);

var date = new Date();
var invalid = new Date(NaN);
var expected = date.getYear();
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test(date), expected);
    var d = new Date();
    shouldBe(test(d), d.getYear());
    shouldBe(isNaN(test(invalid)), true);
}
