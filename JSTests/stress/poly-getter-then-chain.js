function Cons1() {
}
Cons1.prototype.f = 42;

function Cons2() {
}
Cons2.prototype.__defineGetter__("f", function() {
    counter++;
    return 84;
});

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
    test(new Cons2(), 84, counter + 1);
    test(new Cons1(), 42, counter);
}
