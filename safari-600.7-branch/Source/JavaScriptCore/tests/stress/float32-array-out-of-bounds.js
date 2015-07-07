function foo(a) {
    return a[42];
}

noInline(foo);

var shortArray = new Float32Array(10);
var longArray = new Float32Array(100);

function test(array, expected) {
    var result = foo(array);
    if (result != expected)
        throw new Error("bad result: " + result);
}

for (var i = 0; i < 1000; ++i)
    test(shortArray, void 0);

for (var i = 0; i < 100000; ++i)
    test(longArray, 0);

test(shortArray, void 0);

