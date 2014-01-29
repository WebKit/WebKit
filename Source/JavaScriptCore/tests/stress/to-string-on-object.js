function Foo() { }

Foo.prototype.toString = function() { return "hello" };

function foo(o) {
    return String(o);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(new Foo());
    if (typeof result != "string") {
        describe(result);
        throw "Error: result isn't a string";
    }
    if (result != "hello")
        throw "Error: bad result: " + result;
}
