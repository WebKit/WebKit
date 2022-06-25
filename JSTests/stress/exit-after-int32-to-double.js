function foo(x, o) {
    return o.f + x;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(42.5, {f:5});
    if (result != 47.5)
        throw "Error: bad result: " + result;
}

var result = foo("42", {f:5});
if (result != "542")
    throw "Error: bad result: " + result;
