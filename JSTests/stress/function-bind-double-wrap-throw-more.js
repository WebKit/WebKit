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

var bound = test.bind(undefined, 1, 2);
for (var i = 0; i < 100; ++i)
    var bound = bound.bind(undefined);

for (var i = 0; i < 1e6; ++i)
    shouldBe(bound(), 3);
flag = true;
var error = null;
try {
    bound();
} catch (e) {
    error = e;
}
shouldBe(error !== null, true);
shouldBe(error.stack.trim().split('\n').length, 3);
