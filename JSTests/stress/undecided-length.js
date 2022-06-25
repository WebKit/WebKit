function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [];

function test1(array)
{
    return array.length;
}
noInline(test1);
for (var i = 0; i < 1e5; ++i)
    shouldBe(test1(array), 0);

var array = [];
array.ok = 42;

function test2(array)
{
    return array.length;
}
noInline(test2);
for (var i = 0; i < 1e5; ++i)
    shouldBe(test2(array), 0);
