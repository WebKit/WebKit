//@ $skipModes << :lockdown if $buildType == "debug"

"use strict";

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function inlined(cond)
{
    if (!cond)
        throw new Error("OK");
}
function test()
{
    try {
        inlined.apply(undefined, arguments);
    } catch (error) {
        shouldBe(String(error), `Error: OK`);
        shouldBe(arguments.length, 3);
        shouldBe(arguments[0], 0);
        shouldBe(arguments[1], 2);
        shouldBe(arguments[2], 3);
    }
}
noInline(test);

for (var i = 0; i < 1e7; ++i) {
    test(1, 2, 3);
}
test(0, 2, 3);
test(0, 2, 3);
test(0, 2, 3);
test(0, 2, 3);
test(0, 2, 3);
