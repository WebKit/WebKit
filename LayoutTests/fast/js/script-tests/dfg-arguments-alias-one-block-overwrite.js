description(
"Tests that if you alias the arguments in a very small function, arguments simplification still works even if the variable isn't must-aliased."
);

function foo() {
    var args = arguments;
    args = [1, 2, 3];
    return args[0] + args[1] + args[2];
}

var result = "";
for (var i = 0; i < 300; ++i) {
    var a;
    if (i < 200)
        a = i;
    else
        a = "hello";
    var b = i + 1;
    var c = i + 3;
    shouldBe("foo(a, b, c)", "6");
}

