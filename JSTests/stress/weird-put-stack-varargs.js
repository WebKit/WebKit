function baz() {
    if (!foo.arguments[1])
        throw "Error: foo.arguments[1] should be truthy but is falsy: " + foo.arguments[1];
}

noInline(baz);

function foo(a, b) {
    if (a)
        b = 42;
    baz();
}

function fuzz(a, b) {
    return a + b;
}

function bar(array1, array2) {
    fuzz.apply(this, array1);
    foo.apply(this, array2);
}

noInline(bar);

for (var i = 0; i < 100000; ++i)
    bar([false, false], [false, true]);
