function foo(o) {
    return o.f;
}

noInline(foo);

String.prototype.f = 42;
Number.prototype.f = 24;

for (var i = 0; i < 100000; ++i) {
    var result = foo("hello");
    if (result != 42)
        throw "Error: bad result for string: " + result;
    result = foo(13);
    if (result != 24)
        throw "Error: bad result for number: " + result;
    result = foo({f:84});
    if (result != 84)
        throw "Error: bad result for object: " + result;
}

