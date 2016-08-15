function foo(array) {
    return array.byteOffset;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(new Int32Array(100));
    if (result != 0)
        throw "Error: bad result for fast typed array: " + result;
    result = foo(new Int32Array(100000));
    if (result != 0)
        throw "Error: bad result for big typed array: " + result;
    result = foo(new Int32Array(new ArrayBuffer(100), 4, 1));
    if (result != 4)
        throw "Error: bad result for wasteful typed array: " + result;
}
