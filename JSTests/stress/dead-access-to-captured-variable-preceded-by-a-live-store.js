function foo() {
    var x = 42;
    (function() { x = 43; })();
    x++;
    var realResult = x;
    (function() { x = 44; })();
    var fakeResult = x;
    return realResult;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo();
    if (result != 44)
        throw "Error: bad result: " + result;
}
