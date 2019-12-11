function make(value) {
    var result = new Float32Array(1);
    result[0] = value;
    return result;
}

function foo(a, i) {
    return a[i];
}

noInline(foo);

function test(value) {
    var result = foo(make(value), 0);
    if (result != value)
        throw "Error: bad result: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(42);

var result = foo(make(42), 1);
if (result !== void 0)
    throw "Error: bad result: " + result;

Float32Array.prototype[1] = 23;
result = foo(make(42), 1);
if (result !== 23)
    throw "Error: bad result: " + result;
