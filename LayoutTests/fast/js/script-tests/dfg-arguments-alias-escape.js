description(
"Tests aliased uses of 'arguments' that escape."
);

function foo() {
    var result = 0;
    var a = arguments;
    for (var i = 0; i < a.length; ++i)
        result += a[i];
    return [result, a];
}

function bar(x) {
    var result = foo(x);
    return result[0] * result[1][0];
}

silentTestPass = true;
noInline(bar);

for (var i = 0; i < 200; i = dfgIncrement({f:bar, i:i + 1, n:100}))
    shouldBe("bar(42)", "1764");

