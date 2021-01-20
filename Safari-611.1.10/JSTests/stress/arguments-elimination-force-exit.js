function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function strict(flag)
{
    "use strict";
    if (flag)
        return arguments.length + 42;
    return arguments.length;
}
noInline(strict);

function sloppy(flag)
{
    if (flag)
        return arguments.length + 42;
    return arguments.length;
}
noInline(sloppy);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(strict(false), 1);
    shouldBe(sloppy(false), 1);
}
shouldBe(strict(true), 43);
shouldBe(sloppy(true), 43);
