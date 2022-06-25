description(
"Tests a function that might create 'arguments' but doesn't, but does create function.arguments."
);

function bar() {
    return foo.arguments;
}

function foo(p) {
    var x = 42;
    if (p)
        return arguments[0];
    else
        return bar();
}

for (var i = 0; i < 200; ++i) {
    var thingy = foo(false);
    shouldBe("thingy.length", "1");
    shouldBe("thingy[0]", "false");
}

