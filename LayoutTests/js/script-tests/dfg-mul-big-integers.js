description(
"Tests what happens when you multiply large integers in an integer context."
);

function foo(a, b) {
    return (a * b) | 0;
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo(2147483647, 2147483646)", "-2147483648");


