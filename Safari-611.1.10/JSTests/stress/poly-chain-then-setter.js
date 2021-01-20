function Cons1() {
}
Cons1.prototype.f = 42;

function Cons2() {
}
Cons2.prototype.__defineSetter__("f", function(value) {
    counter++;
    this._f = value;
});
Cons2.prototype.__defineGetter__("f", function() { return this._f; });

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
    test(new Cons1(), i, counter);
    test(new Cons2(), i, counter + 1);
}
