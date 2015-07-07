function foo(a, i) {
    return a[i];
}

noInline(foo);

function test(value) {
    var result = foo([value], 0);
    if (result != value)
        throw "Error: bad result: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(42);

var result = foo([42], 1);
if (result !== void 0)
    throw "Error: bad result: " + result;

result = foo([42], 100);
if (result !== void 0)
    throw "Error: bad result: " + result;

result = foo([42], 10000);
if (result !== void 0)
    throw "Error: bad result: " + result;

Array.prototype[10000] = 23;
result = foo([42], 10000);
if (result !== 23)
    throw "Error: bad result: " + result;
