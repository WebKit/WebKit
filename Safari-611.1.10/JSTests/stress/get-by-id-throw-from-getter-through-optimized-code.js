function foo(o) {
    return o.f + 1;
}

noInline(foo);

var shouldThrow = false;

function makeWithGetter() {
    var o = {};
    o.__defineGetter__("f", function() {
        if (shouldThrow)
            throw "hello";
        return 42;
    });
    return o;
}

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:23});
    if (result != 24)
        throw "Error: bad result: " + result;
    result = foo(makeWithGetter());
    if (result != 43)
        throw "Error: bad result: " + result;
}

var didThrow;
try {
    shouldThrow = true;
    foo(makeWithGetter());
} catch (e) {
    didThrow = e;
}

if (didThrow != "hello")
    throw "Error: didn't throw or threw wrong exception: " + didThrow;
