function foo(baz) {
    return bar(baz);
}

function fuzz(baz) {
    return bar(baz);
}

function bar(baz) {
    return baz();
}

function baz1() {
    return 42;
}

function baz2() {
    return 24;
}

noInline(foo);
noInline(fuzz);

for (var i = 0; i < 100000; ++i) {
    var result = foo(baz1);
    if (result != 42)
        throw "Error: bad result: " + result;
    var result = fuzz(baz2);
    if (result != 24)
        throw "Error: bad result: " + result;
}

