function foo(array, v, p) {
    array[0] = 10;
    if (p)
        v = "hello";
    array[0] = v;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var array = [42];
    foo(array, 43, false);
    if (array[0] != 43)
        throw "Error: bad result: " + array;
}

