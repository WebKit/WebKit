description(
"Tests aliased uses of 'arguments'."
);

function foo() {
    var result = 0;
    var a = arguments;
    for (var i = 0; i < a.length; ++i)
        result += a[i];
    return result;
}

function bar(x) {
    return foo(x);
}

silentTestPass = true;
noInline(bar);

for (var i = 0; i < 200; i = dfgIncrement({f:bar, i:i + 1, n:100}))
    shouldBe("bar(42)", "42");

