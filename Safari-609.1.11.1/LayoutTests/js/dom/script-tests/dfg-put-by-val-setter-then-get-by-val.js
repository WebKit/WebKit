description(
"Tests that a GetByVal that accesses a value that was PutByVal'd, but where the PutByVal invoked a setter, works correctly."
);

function foo(a, i, v) {
    a[i] = v;
    return a[i];
}

var array = [];
var thingy;
array.__defineSetter__(
    "-1", function(x) { thingy = x; }
);
array.__defineGetter__(
    "-1", function() { return 42; }
);

for (var i = 0; i < 200; ++i) {
    shouldBe("foo(array, -1, i)", "42");
    shouldBe("thingy", "" + i);
}

