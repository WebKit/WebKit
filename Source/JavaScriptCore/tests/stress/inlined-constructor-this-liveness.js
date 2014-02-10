function Foo(a, b) {
    this.f = a.f;
    this.g = b.f + 1;
}

function foo(a, b) {
    return new Foo(a, b);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:1}, {f:2});
    if (result.f != 1)
        throw "Error: bad result.f: " + result.f;
    if (result.g != 3)
        throw "Error: bad result.g: " + result.g;
}

var result = foo({f:1}, {f:2.5});
if (result.f != 1)
    throw "Error: bad result.f: " + result.f;
if (result.g != 3.5)
    throw "Error: bad result.f: " + result.g;
