function foo() {
    return this.f;
}

noInline(foo);

String.prototype.f = 43;
String.prototype.g = foo;
Number.prototype.f = 78;
Number.prototype.g = foo;

for (var i = 0; i < testLoopCount; ++i) {
    var o = {f:foo};
    var result = o.f();
    if (result != foo)
        throw "Error: bad object result: " + result;
    o = "hello";
    result = o.g();
    if (result != 43)
        throw "Error: bad string result: " + result;
    o = 42;
    result = o.g();
    if (result != 78)
        throw "Error: bad number result: " + result;
}
