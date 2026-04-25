description(
"This tests that inlining correctly handles constant buffers."
);

function foo() {
    return [1, 2, 3, 4];
}

function bar() {
    return foo();
}

for (var i = 0; i < 1000; ++i) {
    bar();
}

for (var i = 0; i < 10; ++i) {
    shouldBe("bar()[0]", "1")
    shouldBe("bar()[1]", "2")
    shouldBe("bar()[2]", "3")
    shouldBe("bar()[3]", "4")
}

var successfullyParsed = true;
