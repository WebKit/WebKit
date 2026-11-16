var count = 0;

function bar(f) {
    if (++count >= 10000)
        throw f;
}

noInline(bar);

function foo(a) {
    var x = a + 1;
    for (;;) {
        bar(function() { return x; });
    }
}

noInline(foo);

try {
    foo(42);
} catch (f) {
    var result = f();
    if (result != 43)
        throw "Error: bad result: " + result;
}

