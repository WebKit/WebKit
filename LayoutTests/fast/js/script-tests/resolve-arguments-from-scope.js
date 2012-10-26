description(
"Tests that doing repeated resolves of 'arguments' from some nested scope doesn't crash."
);

function bar() {
    throw "omg";
}

function foo() {
    try {
        bar();
    } catch (e) {
        return arguments;
    }
}

for (var i = 0; i < 100; ++i) {
    shouldBe("foo(42)[0]", "42");
    shouldBe("foo(42).length", "1");
    shouldBe("foo(42, 23)[1]", "23");
}

