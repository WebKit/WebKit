function foo(a, b) {
    this.f = a;
    this.g = b;
}

function Bar() {
    foo.apply(this, arguments);
}

noInline(Bar);

for (var i = 0; i < 1000000; ++i) {
    var result = new Bar(1, 2);
    if (result.f != 1)
        throw "Error: bad result.f: " + result.f;
    if (result.g != 2)
        throw "Error: bad result.g: " + result.g;
}

