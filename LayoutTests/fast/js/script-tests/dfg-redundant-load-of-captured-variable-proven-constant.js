description(
"Tests that a redundant load of a captured variable that was proven to be a constant doesn't crash the compiler."
);

function foo(o, p) {
    var x = o.f;
    if (p)
        return function() { return x; }
    else {
        var a = x;
        var b = x;
        return [a, b];
    }
}

var o = {f:function() { return 32; }};

for (var i = 0; i < 100; ++i) {
    var expected;
    if (i % 2)
        expected = "\"function () { return x; }\"";
    else
        expected = "\"function () { return 32; },function () { return 32; }\"";
    shouldBe("\"\" + foo(o, i % 2)", expected);
}
