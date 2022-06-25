//@ runDefault("--useConcurrentJIT=0")

noInline(Function.prototype.apply);

function test()
{
    for (var i = 0; i < 1e4; ++i) {
        try {
            Function.prototype.apply.call(WeakMap.bind());
        } catch { }
        try {
            Function.prototype.apply.call(WeakMap);
        } catch { }
    }
    try {
        Function.prototype.apply.call(42);
    } catch { }
}

test();
