function foo(o, p) {
    if (p)
        return o.f;
    else
        return [o * 1.1, o * 1.2, o * 1.3];
}

for (var i = 0; i < 100; ++i)
    foo({f:42}, true);

function bar() {
    var x = 4.5;
    for (var i = 0; i < 10; ++i) {
        x *= 1.1;
        x += 0.05;
        foo(x, false);
    }
    return x * 1.03;
}

noInline(bar);

for (var i = 0; i < 10000; ++i)
    bar();

