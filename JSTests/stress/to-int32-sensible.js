function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// ValueToInt32(DoubleRep)
function toInt32(number)
{
    return (number * 0.5) >> 0;
}
noInline(toInt32);
for (var i = 0; i < 1e5; ++i)
    toInt32(i * 1.0);

function test(number)
{
    return toInt32(number * 2);
}

const INT32_MAX = 2147483647;
const INT32_MIN = -2147483648;

shouldBe(test(INT32_MAX - 1), INT32_MAX - 1);
shouldBe(test(INT32_MAX - 0.5), INT32_MAX - 1);
shouldBe(test(INT32_MAX), INT32_MAX);
shouldBe(test(INT32_MAX + 0.5), INT32_MAX);
shouldBe(test(INT32_MAX + 1), INT32_MIN);

shouldBe(test(INT32_MIN - 1), INT32_MAX);
shouldBe(test(INT32_MIN - 0.5), INT32_MIN);
shouldBe(test(INT32_MIN), INT32_MIN);
shouldBe(test(INT32_MIN + 0.5), INT32_MIN + 1);
shouldBe(test(INT32_MIN + 1), INT32_MIN + 1);

shouldBe(test(Number.EPSILON), 0);
shouldBe(test(Number.NaN), 0);
shouldBe(test(Number.POSITIVE_INFINITY), 0);
shouldBe(test(Number.NEGATIVE_INFINITY), 0);
shouldBe(test(Number.MAX_SAFE_INTEGER), -1);
shouldBe(test(Number.MIN_SAFE_INTEGER), 1);
