function foo(s) {
    return s[-1];
}

noInline(foo);

String.prototype[-1] = "hello";

for (var i = 0; i < 100000; ++i) {
    var result = foo("hello");
    if (result != "hello")
        throw "Error: bad result: " + result;
}

