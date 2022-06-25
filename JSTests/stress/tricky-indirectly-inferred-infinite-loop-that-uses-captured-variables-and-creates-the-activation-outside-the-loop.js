var count = 0;

function bar(f) {
    if (++count < 10)
        return;
    count = 0;
    throw f;
}

noInline(bar);

function fuzz(a) {
    return a != true;
}

function foo(a) {
    var x = a + 1;
    var y = a + 2;
    var f = (function() { return x; });
    while (fuzz(y)) {
        bar(f);
    }
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    try {
        foo(i);
    } catch (f) {
        var result = f();
        if (result != i + 1)
            throw "Error: bad result for i = " + i + ": " + result;
    }
}

