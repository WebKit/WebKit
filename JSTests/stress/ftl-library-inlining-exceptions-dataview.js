function foo(d){
    return d.getInt8(42);
}

noInline(foo);

var d = new DataView(new ArrayBuffer(43));
d.setInt8(42, 43);
for (var i = 0; i < 100000; ++i) {
    var result = foo(d);
    if (result != 43)
        throw "Error: bad result: " + result;
}

for (var i = 0; i < 10; ++i) {
    var didThrow = false;
    try {
        foo(new DataView(new ArrayBuffer(42)));
    } catch (e) {
        didThrow = true;
        if (e.message.indexOf("Out of bounds") < 0)
            throw "Error: bad exception: " + e.message;
    }
    if (!didThrow)
        throw "Error: did not throw";
}
