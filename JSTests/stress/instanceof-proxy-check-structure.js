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

// Break some watchpoints.
var o = {f:42};
o.g = 43;

function foo(o, p, q)
{
    var result = o.f;
    var _ = p instanceof Foo;
    q.f = 11;
    return result + o.f;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:42}, new Bar(), {f:0});
    if (result != 84)
        throw "Error: bad result in loop: " + result;
}

if (numberOfGetPrototypeOfCalls != 10000)
    throw "Error: did not call getPrototypeOf() the right number of times";

var globalO = {f:42};
var didCallGetter = false;
doBadThings = function() {
    delete globalO.f;
    globalO.__defineGetter__("f", function() {
        didCallGetter = true;
        return 43;
    });
};

var result = foo(globalO, new Bar(), {f:0});
if (result != 85)
    throw "Error: bad result at end: " + result;
if (!didCallGetter)
    throw "Error: did not call getter";
if (numberOfGetPrototypeOfCalls != 10001)
    throw "Error: did not call getPrototypeOf() the right number of times at end";
