function Foo(a, b) {
    this.f = a;
    this.g = b;
}

noInline(Foo);

function bar() {
    var result = new Foo(...arguments);
    if (!result)
        throw "Error: bad result: " + result;
    return result;
}

noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = bar(1, 2);
    if (result.f != 1)
        throw "Error: bad result.f: " + result.f;
    if (result.g != 2)
        throw "Error: bad result.g: " + result.g;
}

