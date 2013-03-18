description(
"Tests that we do ToString conversions correctly when String.prototype.valueOf is initially fine but then gets clobbered, and was a dictionary all along."
);

delete String.prototype.trim;
delete String.prototype.toLowerCase;
delete String.prototype.toLocaleUpperCase;

for (var i = 0; i < 1000; ++i)
    String.prototype["a" + i] = 42;

function foo(a) {
    for (var i = 0; i < 100; ++i)
        a = new String(a);
    return a;
}

var expected = "\"hello\"";
for (var i = 0; i < 150; ++i) {
    if (i == 100) {
        String.prototype.toString = function() { return 42; }
        expected = "\"42\"";
    }
    shouldBe("\"\" + foo(\"hello\")", expected);
}

