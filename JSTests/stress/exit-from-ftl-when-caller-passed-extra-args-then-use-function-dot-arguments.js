function foo(a, b) {
    var result = a + b;
    bar();
    return result;
}

var capturedArgs;
function bar() {
    capturedArgs = foo.arguments;
}

noInline(foo);
noInline(bar);

function arraycmp(a, b) {
    if (a.length != b.length)
        return false;
    for (var i = 0; i < a.length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

for (var i = 0; i < 10000; ++i) {
    var result = foo(1, 2, 3, 4, 5, 6);
    if (result != 3)
        throw "Error: bad result in loop: " + result;
    if (!arraycmp(capturedArgs, [1, 2, 3, 4, 5, 6]))
        throw "Error: bad captured arguments in loop: " + capturedArgs;
}

var result = foo(2000000000, 2000000000, 3, 4, 5, 6);
if (result != 4000000000)
    throw "Error: bad result at end: " + result;
if (!arraycmp(capturedArgs, [2000000000, 2000000000, 3, 4, 5, 6]))
    throw "Error: bad captured arguments at end: " + Array.prototype.join.apply(capturedArgs, ",");
