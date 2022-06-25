function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: expected:(${expected}),actual:(${actual})`);
}

function toStringLeft(num)
{
    return num + 'Cocoa';
}
noInline(toStringLeft);

function toStringRight(num)
{
    return 'Cocoa' + num;
}
noInline(toStringRight);

function toStringLeftEmpty(num)
{
    return num + '';
}
noInline(toStringLeftEmpty);

function toStringRightEmpty(num)
{
    return '' + num;
}
noInline(toStringRightEmpty);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(toStringLeft(2), `2Cocoa`);
    shouldBe(toStringRight(2), `Cocoa2`);
    shouldBe(toStringLeftEmpty(2), `2`);
    shouldBe(toStringRightEmpty(2), `2`);
    shouldBe(toStringLeft(42), `42Cocoa`);
    shouldBe(toStringRight(42), `Cocoa42`);
    shouldBe(toStringLeftEmpty(42), `42`);
    shouldBe(toStringRightEmpty(42), `42`);
}
