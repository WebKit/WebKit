description(
"Tests that we do ToString conversions correctly when String.prototype.valueOf is initially fine but then gets clobbered."
);

function foo(a) {
    for (var i = 0; i < 100; ++i)
        a = new String(a);
    return a;
}

var expected = "\"hello\"";
for (var i = 0; i < 150; ++i) {
    if (i == 100) {
        String.prototype.valueOf = function() { return 42; }
        expected = "\"42\"";
    }
    shouldBe("\"\" + foo(\"hello\")", expected);
}

