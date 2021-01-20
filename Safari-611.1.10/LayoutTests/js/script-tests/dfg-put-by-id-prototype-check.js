description(
"Tests that the DFG CFA is not overzealous in removing prototype structure checks for put_by_id transitions."
);

function foo(a, b)
{
    a.f = b;
}

var Empty = "";

function Foo() {
}

var stuff;

for (var i = 0; i < 1000; ++i) {
    if (i >= 900)
        Foo.prototype.__defineSetter__("f", function(value) { stuff = value; });
    
    var o = new Foo();
    eval(Empty + "foo(o, i)");
    if (i >= 900) {
        shouldBe("stuff", "" + i);
        shouldBe("o.f", "" + (void 0));
    } else
        shouldBe("o.f", "" + i);
}

