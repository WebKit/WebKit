function foo(o) {
    return o[0];
}

noInline(foo);

function isBigEndian() {
    var word = new Int16Array(1);
    word[0] = 1;
    var bytes = new Int8Array(word.buffer);
    return !bytes[0];
}

function test(a, b, x) {
    var intArray = new Int32Array(2);
    intArray[0] = a;
    intArray[1] = b;
    var floatArray = new Float64Array(intArray.buffer);
    var element = foo(floatArray);
    var result = element + 1;
    if (("" + result) != ("" + x))
        throw "Error: bad result for " + a + ", " + b + ": " + result + ", but expected: " + x + "; loaded " + element + " from the array";
}

for (var i = 0; i < 100000; ++i)
    test(0, 0, 1);

if (isBigEndian()) {
    test(0xFFFF0000, 0, 0/0);
    test(0, 0xFFFF0000, 1);
} else {
    test(0xFFFF0000, 0, 1);
    test(0, 0xFFFF0000, 0/0);
}
