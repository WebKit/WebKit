function foo(p) {
    if (p) {
        var x = 42;
        (function() { x = 43; })();
        x++;
        var realResult = x;
        (function() { x = 44; })();
        var fakeResult = x;
        return realResult;
    }
    var y = 45;
    (function() { y = 46; })();
    y++;
    var realResult2 = y;
    (function() { y = 47; })();
    var fakeResult2 = y;
    return realResult2;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(i & 1);
    if (result != ((i & 1) ? 44 : 47))
        throw "Error: bad result with i = " + i + ": " + result;
}
