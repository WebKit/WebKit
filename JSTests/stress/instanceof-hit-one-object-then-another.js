function foo(o, p)
{
    return o instanceof p;
}

noInline(foo);

class Foo { }
class Bar { }

for (var i = 0; i < 10000; ++i) {
    var result = foo(new Foo(), Foo);
    if (!result)
        throw "Error: bad result in loop: " + result;
}

var result = foo(new Foo(), Bar);
if (result)
    throw "Error: bad result at end: " + result;
