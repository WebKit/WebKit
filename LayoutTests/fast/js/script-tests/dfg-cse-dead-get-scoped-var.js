description(
"Tests that CSE doesn't try to match against a dead GetScopedVar."
);

function foo(a) {
    var x = a;
    return function(p) {
        if (p) {
            var tmp = x;
            return x;
        }
        return 42;
    };
}

for (var i = 0; i < 1000; ++i)
    shouldBe("foo(i)(false)", "42");

