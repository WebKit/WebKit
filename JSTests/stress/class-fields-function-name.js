//@ requireOptions("--useClassFields=true")

function assert(a, message) {
    if (!a)
        throw new Error(message);
}

class A {
    foo = function() { };
    bar = class {};
    baz = "test";
}

var a = new A();
assert(a.foo.name == "foo");
assert(a.bar.name == "bar");
assert(a.baz.name == undefined);
