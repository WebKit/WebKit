function foo(o) {
    return o.f + 1;
}

Number.prototype.f = 42;

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(23);
    if (result != 43)
        throw "Error: bad result: " + result;
    result = foo({f:25});
    if (result != 26)
        throw "Error: bad result: " + result;
    result = foo({g:12, f:13});
    if (result != 14)
        throw "Error: bad result: " + result;
}

var didThrow;
try {
    foo(void 0);
} catch (e) {
    didThrow = e;
}

if (!didThrow || didThrow.toString().indexOf("TypeError:") != 0)
    throw "Error: didn't throw or threw wrong exception: " + didThrow;
