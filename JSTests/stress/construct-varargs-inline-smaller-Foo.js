function Foo(a, b) {
    var array = [];
    for (var i = 0; i < arguments.length; ++i)
        array.push(arguments[i]);
    this.f = array;
}

function bar(array) {
    return new Foo(...array);
}

noInline(bar);

function checkEqual(a, b) {
    if (a.length != b.length)
        throw "Error: bad value of c, length mismatch: " + a + " versus " + b;
    for (var i = a.length; i--;) {
        if (a[i] != b[i])
            throw "Error: bad value of c, mismatch at i = " + i + ": " + a + " versus " + b;
    }
}

function test(array) {
    var expected = array;
    var actual = bar(array).f;
    checkEqual(actual, expected);
}

for (var i = 0; i < 10000; ++i) {
    var array = [];
    for (var j = 0; j < i % 6; ++j)
        array.push(j);
    test(array);
}

