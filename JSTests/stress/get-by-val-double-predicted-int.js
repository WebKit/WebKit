function foo(a, i) {
    var x = a[i];
    predictInt32(x);
    return x + 2000000000;
}

noInline(foo);

var array = [2000000000.5];

for (var i = 0; i < 1000000; ++i) {
    var result = foo(array, 0);
    if (result != 4000000000.5)
        throw "Error: bad result: " + result;
}

