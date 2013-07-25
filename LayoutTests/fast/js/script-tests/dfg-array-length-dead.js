description(
"Tests that an array length access being dead does not result in register allocation failures."
);

function foo(x) {
    var y = x.f.length;
    return 42;
}

noInline(foo);
silentTestPass = true;

for (var i = 0; i < 2; i = dfgIncrement({f:foo, i:i + 1, n:1})) {
    shouldBe("foo({f:[]})", "42");
}

