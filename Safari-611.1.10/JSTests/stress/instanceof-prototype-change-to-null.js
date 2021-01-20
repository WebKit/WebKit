function foo(o, p)
{
    return o instanceof p;
}

noInline(foo);

class Foo { }

function Bar() { }
Bar.prototype = new Foo();

new Foo().thingy = 42;

for (var i = 0; i < 10000; ++i) {
    var result = foo(new Bar(), Foo);
    if (!result)
        throw "Error: bad result in loop: " + result;
}

Bar.prototype.__proto__ = null;

var result = foo(new Bar(), Foo);
if (result)
    throw "Error: bad result at end: " + result;
