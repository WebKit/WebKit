function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var flag = false;
function throwable()
{
    if (flag)
        throw new Error();
}
noInline(throwable);

function test(a, b)
{
    throwable();
    return a + b;
}
var bound1 = test.bind(undefined, 1);
var bound2 = bound1.bind(undefined, 2);

for (var i = 0; i < 1e6; ++i)
    shouldBe(bound2(), 3);
flag = true;
var error = null;
try {
    bound2();
} catch (e) {
    error = e;
}
shouldBe(error !== null, true);
shouldBe(error.stack.trim().split('\n').length, 3);
