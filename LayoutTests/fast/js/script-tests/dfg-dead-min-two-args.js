description(
"Tests that a dead use of Math.min(a,b) at least speculates that its arguments are indeed numbers."
);

function foo(a, b) {
    Math.min(a.f, b.f);
    return 100;
}

function bar(a, b) {
    Math.min(a.f, b.f);
    return 100;
}

var x = {f:42};
var y = {f:43};
var ok = null;
var expected = 42;
var empty = "";

for (var i = 0; i < 200; ++i) {
    if (i == 150) {
        x = {f:{valueOf:function(){ ok = i; return 37; }}};
        expected = 37;
    }
    var result = eval(empty + "foo(x, y)");
    if (i >= 150)
        shouldBe("ok", "" + i);
    shouldBe("result", "100");
}

x = {f:42};
y = {f:43};
ok = null;
expected = 42;

for (var i = 0; i < 200; ++i) {
    if (i == 150) {
        y = {f:{valueOf:function(){ ok = i; return 37; }}};
        expected = 37;
    }
    var result = eval(empty + "bar(x, y)");
    if (i >= 150)
        shouldBe("ok", "" + i);
    shouldBe("result", "100");
}

