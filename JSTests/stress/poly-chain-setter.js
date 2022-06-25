function Cons() {
}
Cons.prototype.__defineSetter__("f", function(value) {
    counter++;
    this._f = value;
});
Cons.prototype.__defineGetter__("f", function() { return this._f; });

function foo(o, v) {
    o.f = v;
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
    test(new Cons(), i, counter + 1);
    
    var o = new Cons();
    o.g = 54;
    test(o, i, counter + 1);
}
