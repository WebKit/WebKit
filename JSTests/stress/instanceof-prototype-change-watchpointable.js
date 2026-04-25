function foo(o, p)
{
    return o instanceof p;
}

noInline(foo);

class Foo { }

class Bar extends Foo { }

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo(new Bar(), Foo);
    if (!result)
        throw "Error: bad result in loop: " + result;
}

Bar.prototype.__proto__ = {};

var result = foo(new Bar(), Foo);
if (result)
    throw "Error: bad result at end: " + result;
