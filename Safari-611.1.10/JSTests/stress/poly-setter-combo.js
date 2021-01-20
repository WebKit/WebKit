function Cons1() {
}
Cons1.prototype.f = 42;

function Cons2() {
    this._values = []
}
Cons2.prototype.__defineSetter__("f", function(value) {
    counter++;
    this._f = value;
    this._values[value] = 1;
});
Cons2.prototype.__defineGetter__("f", function() { return this._f; });

function Cons3() {
}
Cons3.prototype.f = 42;
Cons3.prototype.g = 43;

function Cons4() {
    this._values = []
}
Cons4.prototype.g = 16;
Cons4.prototype.__defineSetter__("f", function(value) {
    counter++;
    this._f = value;
    this._values[value] = 1;
});
Cons4.prototype.__defineGetter__("f", function() { return this._f; });

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

function runTestWithConstructors(constructor1, constructor2) {
    for (var i = 0; i < 5000; ++i) {
        test(new constructor1(), i, counter);
        test(new constructor2(), i, counter + 1);

        var o = {};
        o.__defineGetter__("f", function() {
            counter++;
            return 84;
        });
        test(o, 84, counter + 1);

        var o = {};
        o.__defineSetter__("f", function(value) {
            this._f = value;
            counter++;
        });
        o.__defineGetter__("f", function() { return this._f; });
        test(o, i, counter + 1);

        test({f: 42}, i, counter);
    }
}

for (var i = 0; i < 2; ++i) {
    runTestWithConstructors(Cons1, Cons2);
    runTestWithConstructors(Cons3, Cons2);
    runTestWithConstructors(Cons1, Cons4);
    runTestWithConstructors(Cons3, Cons4);
}
