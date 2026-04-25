//@ runDefault("--useConcurrentJIT=0")

function call(func)
{
    "use strict";
    return func();
}
noInline(call);

function test()
{
    for (var i = 0; i < 1e4; ++i) {
        try {
            call(WeakMap);
        } catch {
        }
        try {
            call(function () { });
        } catch {
        }
        call((function () { }).bind());
        call((function () { }).bind());
        call((function () { }).bind());
    }
}

test();
