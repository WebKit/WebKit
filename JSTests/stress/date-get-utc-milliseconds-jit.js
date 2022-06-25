function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(date) {
    return date.getUTCMilliseconds();
}
noInline(test);

var date = new Date();
var invalid = new Date(NaN);
var expected = date.getUTCMilliseconds();
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test(date), expected);
    var d = new Date();
    shouldBe(test(d), d.getUTCMilliseconds());
    shouldBe(isNaN(test(invalid)), true);
}
