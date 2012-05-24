description(
"Tests uses of 'arguments' that are aliased but span code origins."
);

function foo() {
    return arguments;
}

function bar(a) {
    var result = 0;
    for (var i = 0; i < a.length; ++i)
        result += a[i];
    return result;
}

function baz(x) {
    return bar(foo(x));
}

for (var i = 0; i < 200; ++i)
    shouldBe("baz(42)", "42");
