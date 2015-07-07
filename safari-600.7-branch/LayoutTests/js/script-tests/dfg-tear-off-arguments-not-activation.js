description(
"Tests the case that a function that might create an activation has not actually created one, but has created its arguments."
);

function bar() {
    return foo.arguments;
}

function foo(p) {
    var x = 42;
    if (p)
        return (function() { return x; });
    else
        return bar();
}

for (var i = 0; i < 200; ++i) {
    var thingy = foo(false);
    shouldBe("thingy.length", "1");
    shouldBe("thingy[0]", "false");
}

