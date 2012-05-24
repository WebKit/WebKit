description(
"Tests aliased uses of 'arguments' that have an unexpected escape."
);

function baz() {
    return foo.arguments;
}

function foo() {
    var result = 0;
    var a = arguments;
    for (var i = 0; i < a.length; ++i) {
        result += a[i];
        result += baz()[0];
    }
    return result;
}

function bar(x) {
    return foo(x);
}

for (var i = 0; i < 200; ++i)
    shouldBe("bar(42)", "84");
