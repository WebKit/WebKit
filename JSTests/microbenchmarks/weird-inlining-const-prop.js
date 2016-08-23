function foo(o, p, q) {
    var x = o[0];
    var y;
    if (p) {
        x();
        if (q) {
            x();
            y = 42;
        } else {
            x();
            y = 11;
        }
    } else
        y = 23;
    o[1]++;
    return x;
}

function bar(o, p, q) {
    var x = o[0];
    var y;
    if (p)
        y = 23;
    else {
        x();
        if (q) {
            x();
            y = 42;
        } else {
            x();
            y = 11;
        }
    }
    o[1]++;
    return x;
}

function fuzz() { }

noInline(foo);
noInline(bar);

function testImpl(f, x, p) {
    var result = f([fuzz, x], p, false);
    if (result != fuzz)
        throw "Error: bad result: " + result;
}

function test(x, p) {
    testImpl(foo, x, p);
    testImpl(bar, x, !p);
}

for (var i = 0; i < 10000; ++i)
    test(0, true);

for (var i = 0; i < 10000; ++i)
    test(0, false);

test(0.5, true);
