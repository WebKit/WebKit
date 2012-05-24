description(
"Tests functions that use 'arguments' in both an aliased and a non-aliased way."
);

function foo() {
    var result = 0;
    var a = arguments;
    for (var i = 0; i < a.length; ++i)
        result += arguments[i];
    return result;
}

function bar(x) {
    return foo(x);
}

for (var i = 0; i < 200; ++i)
    shouldBe("bar(42)", "42");

