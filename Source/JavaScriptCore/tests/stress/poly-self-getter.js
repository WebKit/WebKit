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

function getter() {
    counter++;
    return 84;
}

for (var i = 0; i < 100000; ++i) {
    var o = {};
    o.__defineGetter__("f", getter);
    test(o, 84, counter + 1);

    var o = {};
    o.__defineGetter__("f", getter);
    o.g = 54;
    test(o, 84, counter + 1);
}
