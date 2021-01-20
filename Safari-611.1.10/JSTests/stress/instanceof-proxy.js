class Foo { }

function Bar() { }

var numberOfGetPrototypeOfCalls = 0;

var doBadThings = function() { };

Bar.prototype = new Proxy(
    {},
    {
        getPrototypeOf()
        {
            numberOfGetPrototypeOfCalls++;
            doBadThings();
            return Foo.prototype;
        }
    });

var o = {f:42};

function foo(o, p)
{
    var result = o.f;
    var _ = p instanceof Foo;
    return result + o.f;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:42}, new Bar());
    if (result != 84)
        throw "Error: bad result in loop: " + result;
}

var globalO = {f:42};
var didCallGetter = false;
doBadThings = function() {
    globalO.f = 43;
};

var result = foo(globalO, new Bar());
if (result != 85)
    throw "Error: bad result at end: " + result;
if (numberOfGetPrototypeOfCalls != 10001)
    throw "Error: did not call getPrototypeOf() the right number of times at end";
