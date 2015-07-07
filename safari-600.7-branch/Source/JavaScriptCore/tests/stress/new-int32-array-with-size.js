function foo(x) {
    return new Array(x);
}

noInline(foo);

function test(size) {
    var result = foo(size);
    if (result.length != size)
        throw "Error: bad result: " + result;
    var sawThings = false;
    for (var s in result)
        sawThings = true;
    if (sawThings)
        throw "Error: array is in bad state: " + result;
    result[0] = 42;
    if (result[0] != 42)
        throw "Error: array is in wierd state: " + result;
}

for (var i = 0; i < 100000; ++i) {
    test(0);
    test(1);
    test(42);
}
