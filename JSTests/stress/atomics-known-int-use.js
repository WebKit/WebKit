// Break type inference.
var o = {f: 42.5};

function foo(a, i) {
    return Atomics.exchange(a, i.f, 42);
}

noInline(foo);

var array = new Int32Array(new SharedArrayBuffer(4));

for (var i = 0; i < 10000; ++i) {
    array[0] = 13;
    var result = foo(array, {f: 0});
    if (result != 13)
        throw "Error in loop: bad result: " + result;
    if (array[0] != 42)
        throw "Error in loop: bad value in array: " + array[0];
}

var success = false;
try {
    array[0] = 14;
    var result = foo(array, {f: 42.5});
    success = true;
} catch (e) {
    if (e.name != "RangeError")
        throw "Error: bad error type: " + e;
}
if (success)
    throw "Error: expected to fail, but didn't."
