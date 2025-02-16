function foo(p) {
    var o = {};
    if (p) {
        for (var i = 0; i < 100; ++i)
            bar(o);
    }
    return 42;
}

function bar() {
}

noInline(foo);
noInline(bar);

for (var i = 0; i < testLoopCount; ++i)
    foo(true);
