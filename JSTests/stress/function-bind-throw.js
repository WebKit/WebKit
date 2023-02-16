function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var flag = false;
function throwable()
{
    if (flag)
        throw new Error("expected");
}
noInline(throwable);

function inner()
{
    throwable();
}
var bound = inner.bind(1, 2, 3);

function test()
{
    bound();
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();

var length = 0;
try {
    flag = true;
    test();
} catch (error) {
    length = error.stack.trim().split('\n').length;
}
shouldBe(length, 4);
