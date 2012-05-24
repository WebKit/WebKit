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

for (var i = 0; i < 200; ++i)
    shouldBe("bar(42)", "42");

