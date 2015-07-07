function foo(o) {
    return o[0];
}

noInline(foo);

function test(a, x) {
    var intArray = new Int32Array(1);
    intArray[0] = a;
    var floatArray = new Float32Array(intArray.buffer);
    var element = foo(floatArray);
    var result = element + 1;
    if (("" + result) != ("" + x))
        throw "Error: bad result for " + a + ": " + result + ", but expected: " + x + "; loaded " + element + " from the array";
}

for (var i = 0; i < 100000; ++i)
    test(0, 1);

test(0xFFFF0000, 0/0);
