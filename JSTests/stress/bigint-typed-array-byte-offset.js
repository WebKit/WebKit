function foo(array) {
    return array.byteOffset;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(new BigInt64Array(100));
    if (result != 0)
        throw "Error: bad result for fast typed array: " + result;
    result = foo(new BigInt64Array(100000));
    if (result != 0)
        throw "Error: bad result for big typed array: " + result;
    result = foo(new BigInt64Array(new ArrayBuffer(100), 8, 1));
    if (result != 8)
        throw "Error: bad result for wasteful typed array: " + result;
}
