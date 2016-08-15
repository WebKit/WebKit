function foo(o, prototype) {
    return o instanceof prototype;
}

noInline(foo);

function test(o, prototype, expected) {
    var actual = foo(o, prototype);
    if (actual != expected)
        throw new Error("bad result: " + actual);
}

function Foo() { }

function Bar() { }
Bar.prototype = new Foo();

for (var i = 0; i < 10000; ++i) {
    test({}, Object, true);
    test({}, Array, false);
    test({}, String, false);
    test({}, Foo, false);
    test({}, Bar, false);
    test([], Object, true);
    test([], Array, true);
    test([], String, false);
    test([], Foo, false);
    test([], Bar, false);
    test(new Foo(), Object, true);
    test(new Foo(), Array, false);
    test(new Foo(), String, false);
    test(new Foo(), Foo, true);
    test(new Foo(), Bar, false);
    test(new Bar(), Object, true);
    test(new Bar(), Array, false);
    test(new Bar(), String, false);
    test(new Bar(), Foo, true);
    test(new Bar(), Bar, true);
}
