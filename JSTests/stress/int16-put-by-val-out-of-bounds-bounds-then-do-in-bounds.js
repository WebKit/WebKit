function foo(a) {
    a[42] = 95010;
}

noInline(foo);

function test(length, expected) {
    var a = new Int16Array(length);
    foo(a);
    var result = a[42];
    if (result != expected)
        throw "Error: bad value at a[42]: " + result;
}

for (var i = 0; i < testLoopCount; ++i)
    test(10, void 0);

test(100, 29474);

