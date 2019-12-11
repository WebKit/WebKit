function foo(o) {
    return o.length;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var a = [1];
    a.f = 42;
    foo(a);
}

var result = foo({});
if (result !== void 0)
    throw "Error: bad result: " + result;
