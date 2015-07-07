function foo(o) {
    return bar(o);
}

function fuzz(o) {
    return bar(o);
}

function bar(o) {
    return o.f;
}

noInline(foo);
noInline(fuzz);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:42});
    if (result != 42)
        throw "Error: bad result: " + result;
    var result = fuzz({g:23, f:24});
    if (result != 24)
        throw "Error: bad result: " + result;
}

