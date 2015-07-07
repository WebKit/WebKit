function foo(a) {
    var result = a[0];
    if (result)
        result += a[1];
    if (result)
        result += a[2];
    if (result)
        result += a[3];
    if (result)
        result += a[4];
    return result;
}

var result = 0;

for (var i = 0; i < 100000; ++i) {
    var array = [1, 2, 3, 4, 5];
    if (i & 1)
        array.f = 42;
    result += foo(array);
}

if (result != 1500000)
    throw "Error: bad result: " + result;
