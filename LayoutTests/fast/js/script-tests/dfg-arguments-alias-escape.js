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

for (var i = 0; i < 200; ++i)
    shouldBe("bar(42)", "1764");

