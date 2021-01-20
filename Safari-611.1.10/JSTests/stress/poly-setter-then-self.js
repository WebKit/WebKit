function foo(o, value) {
    o.f = value;
    return o.f;
}

noInline(foo);

var counter = 0;

function test(o, value, expectedCount) {
    var result = foo(o, value);
    if (result != value)
        throw new Error("Bad result: " + result);
    if (counter != expectedCount)
        throw new Error("Bad counter value: " + counter);
}

for (var i = 0; i < 100000; ++i) {
    var o = {};
    o.__defineSetter__("f", function(value) {
        counter++;
        this._f = value;
    });
    o.__defineGetter__("f", function() { return this._f; });
    test(o, i, counter + 1);

    test({f: 42}, i, counter);
}
