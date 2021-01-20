function foo(o) {
    return o.f;
}

noInline(foo);

function makeWithGetter() {
    var o = {};
    o.__defineGetter__("f", function() {
        throw "hello";
    });
    return o;
}

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:23});
    if (result != 23)
        throw "Error: bad result: " + result;
    result = foo({g:12, f:13});
    if (result != 13)
        throw "Error: bad result: " + result;
    result = foo({g:12, h:13, f:14});
    if (result != 14)
        throw "Error: bad result: " + result;
}

var didThrow;
try {
    foo(makeWithGetter());
} catch (e) {
    didThrow = e;
}

if (didThrow != "hello")
    throw "Error: didn't throw or threw wrong exception: " + didThrow;
