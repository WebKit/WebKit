function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var k = -1;
var o1 = {};
o1[k] = true;
var o2 = {};

function test1(o)
{
    return k in o;
}
noInline(test1);

function test2(o)
{
    return k in o;
}
noInline(test2);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test1(o1), true);
for (var i = 0; i < 1e6; ++i)
    shouldBe(test1(o2), false);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test2(o2), false);
for (var i = 0; i < 1e6; ++i)
    shouldBe(test2(o1), true);
