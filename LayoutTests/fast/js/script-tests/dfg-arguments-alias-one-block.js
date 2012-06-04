description(
"Tests that if you alias the arguments in a very small function, arguments simplification still works."
);

function foo() {
    var args = arguments;
    return args[0] + args[1];
}

for (var i = 0; i < 200; ++i)
    shouldBe("foo(i, i + 1)", "" + (i + i + 1));
