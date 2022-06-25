function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorCondition) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (!errorCondition(error))
        throw new Error(`bad error: ${String(error)}`);
}

function strict(flag)
{
    "use strict";
    if (flag)
        throw arguments;
    return arguments.length;
}
noInline(strict);

function sloppy(flag)
{
    if (flag)
        throw arguments;
    return arguments.length;
}
noInline(sloppy);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(strict(false), 1);
    shouldBe(sloppy(false), 1);
}
function isArguments(arg)
{
    shouldBe(String(arg), `[object Arguments]`);
    shouldBe(arg.length, 1);
    shouldBe(arg[0], true);
    return true;
}
shouldThrow(() => strict(true), isArguments);
shouldThrow(() => sloppy(true), isArguments);
