function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(date) {
    return date.getUTCFullYear();
}
noInline(test);

var date = new Date();
var invalid = new Date(NaN);
// A negative year is sign-extended out of the packed breakdown, so it also checks that the JIT
// boxes it canonically: `!==` compares the raw JSValue bits.
var negative = new Date(Date.UTC(-1, 5, 15));
var expected = date.getUTCFullYear();
var negativeExpected = negative.getUTCFullYear();
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(date), expected);
    shouldBe(test(negative), negativeExpected);
    var d = new Date();
    shouldBe(test(d), d.getUTCFullYear());
    shouldBe(isNaN(test(invalid)), true);
}
