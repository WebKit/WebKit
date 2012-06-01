description(
"Tests stability when the CFA finds a constant in block-local data flow over captured variables but the CSE doesn't because of differences in heap modeling."
);

function foo(a, b) {
    var x = a + b;
    var y = a - b;
    function bar() {
        var m = x + y;
        var n = x - y;
        function baz() {
            return m + n;
        }
        m = 23;
        if (m < n) {
            m = 12;
            y = 72; // PutScopedVar appears to clobber world in CSE but only clobbers structures in CFA.
            return m + 6 + n + baz();
        } else
            return 91;
    }
    x = 42;
    if (y < 0) {
        return x + 5 + y + bar();
    } else
        return 73;
}

for (var i = 0; i < 200; ++i)
    shouldBe("foo(i, i + 1)", "162");

