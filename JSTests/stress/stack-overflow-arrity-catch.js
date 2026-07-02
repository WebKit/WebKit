function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function funcWithManyArgs(
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
    a, a, a, a, a, a, a, a, a, a,
)
{
}
noInline(funcWithManyArgs);

var gotRightCatch = false;

function test(date)
{
    try {
        test(new Date());
    } catch { }
    try {
        funcWithManyArgs(1, 2, 3, 4, 5, 6);
    } catch {
        gotRightCatch = true;
    }
}

test();

shouldBe(gotRightCatch, true);
