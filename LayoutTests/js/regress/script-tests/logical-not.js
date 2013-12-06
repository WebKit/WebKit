function foo(v) {
    return !v;
}

noInline(foo);

var array = ["foo", 42, null, {}, true, false];
var expected = [false, false, true, false, false, true];

for (var i = 0; i < 200000; ++i) {
    var result = foo(array[i % array.length]);
    if (result !== expected[i % array.length])
        throw "Error: bad result at " + i + ": " + result;
}
