description(
"Tests what happens when you have a NewArray that uses an integer constant as a double constant, and then that constant is subsequently used as an integer constant."
);

function bar(x) {
    return [[x, 1.5], [x, 0]];
}

function foo() {
    return bar(1)[1][0];
}

silentTestPass = true;
noInline(foo);

for (var i = 0; i < 2; i = dfgIncrement({f:foo, i:i + 1, n:1}))
    shouldBe("foo()", "1");


