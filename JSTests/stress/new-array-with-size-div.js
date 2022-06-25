function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(length, number)
{
    return new Array(length / number);
}
noInline(test);

var value = 0;
var number = 0;
for (var i = 0; i < 1e4; ++i) {
    value = i * 2;
    number = 2;
    if (i === (1e4 - 1)) {
        value = 0;
        number = -1;
    }
    shouldBe(test(value, number).length, (value / number));
}
