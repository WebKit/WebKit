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
        return inlined.apply(undefined, arguments);
    } catch (error) {
        shouldBe(String(error), `Error: OK`);
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
