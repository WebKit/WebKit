function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var INT32_MIN = -2147483648;
var INT32_MIN_IN_UINT32 = 0x80000000;
var o1 = [];
o1[INT32_MIN_IN_UINT32] = true;
ensureArrayStorage(o1);

function test1(o, key)
{
    return key in o;
}
noInline(test1);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test1(o1, INT32_MIN), false);
