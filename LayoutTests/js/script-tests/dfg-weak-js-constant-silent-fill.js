description(
"Tests that DFG silent spill and fill of WeakJSConstants does not result in nonsense."
);

function foo(a, b, c, d)
{
    a.f = 42; // WeakJSConstants corresponding to the o.f transition get created here.
    var x = !d; // Silent spilling and filling happens here.
    b.f = x; // The WeakJSConstants get reused here.
    var y = !d; // Silent spilling and filling happens here.
    c.f = y; // The WeakJSConstants get reused here.
}

var Empty = "";

for (var i = 0; i < 1000; ++i) {
    var o1 = new Object();
    var o2 = new Object();
    var o3 = new Object();
    eval(Empty + "foo(o1, o2, o3, \"stuff\")");
    shouldBe("o1.f", "42");
    shouldBe("o2.f", "false");
    shouldBe("o3.f", "false");
}

