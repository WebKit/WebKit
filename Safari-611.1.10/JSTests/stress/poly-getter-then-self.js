function foo(o) {
    return o.f;
}

noInline(foo);

var counter = 0;

function test(o, expected, expectedCount) {
    var result = foo(o);
    if (result != expected)
        throw new Error("Bad result: " + result);
    if (counter != expectedCount)
        throw new Error("Bad counter value: " + counter);
}

for (var i = 0; i < 100000; ++i) {
    var o = {};
    o.__defineGetter__("f", function() {
        counter++;
        return 84;
    });
    test(o, 84, counter + 1);

    test({f: 42}, 42, counter);
}
