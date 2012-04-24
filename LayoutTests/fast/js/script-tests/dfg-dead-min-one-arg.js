description(
"Tests that a dummy use of Math.min(a) at least speculates that its argument is indeed a number."
);

function foo(a) {
    return Math.min(a.f);
}

var x = {f:42};
var ok = null;
var expected = 42;
var empty = "";

for (var i = 0; i < 200; ++i) {
    if (i == 150) {
        x = {f:{valueOf:function(){ ok = i; return 37; }}};
        expected = 37;
    }
    var result = eval(empty + "foo(x)");
    if (i >= 150)
        shouldBe("ok", "" + i);
    shouldBe("result", "" + expected);
}

