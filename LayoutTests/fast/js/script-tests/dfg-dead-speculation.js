description(
"Tests that the DFG will still perform speculations on dead things."
);

function foo(a, b) {
    var x = a.f - b;
    return 10;
}

var thingy = {f:42};
var variable = 84;
var expected = 84;

for (var i = 0; i < 200; ++i) {
    if (i == 150) {
        thingy = {f:{valueOf:function(){ variable = 24; return 5; }}};
        expected = 24;
    }
    shouldBe("foo(thingy, i)", "10");
    shouldBe("variable", "" + expected);
}

