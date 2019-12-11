function foo(o, p)
{
    return o instanceof p;
}

noInline(foo);

class Foo { }
class Bar { }
class Baz { }

for (var i = 0; i < 10000; ++i) {
    var result = foo(new Foo(), Foo);
    if (!result)
        throw "Error: bad result in loop (1): " + result;
    var result = foo(new Foo(), Bar);
    if (result)
        throw "Error: bad result in loop (2): " + result;
}

var result = foo(new Foo(), Baz);
if (result)
    throw "Error: bad result at end: " + result;
