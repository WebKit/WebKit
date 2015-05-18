function foo(a, b) {
    var array = [];
    for (var i = 0; i < arguments.length; ++i)
        array.push(arguments[i]);
    return {a:a + 1, b:b, c:array};
}

function bar(array) {
    return foo.apply(this, array);
}

noInline(bar);

function checkEqual(a, b) {
    if (a.a != b.a)
        throw "Error: bad value of a: " + a.a + " versus " + b.a;
    if (a.b != b.b)
        throw "Error: bad value of b: " + a.b + " versus " + b.b;
    if (a.c.length != b.c.length)
        throw "Error: bad value of c, length mismatch: " + a.c + " versus " + b.c;
    for (var i = a.c.length; i--;) {
        if (a.c[i] != b.c[i])
            throw "Error: bad value of c, mismatch at i = " + i + ": " + a.c + " versus " + b.c;
    }
}

function test(array) {
    var expected = {a:array[0] + 1, b:array[1], c:array};
    var actual = bar(array);
    checkEqual(actual, expected);
}

for (var i = 0; i < 10000; ++i) {
    var array = [];
    for (var j = 0; j < 1 + (i % 5); ++j)
        array.push(j);
    test(array);
}

var array = [2147483647];
test(array);
