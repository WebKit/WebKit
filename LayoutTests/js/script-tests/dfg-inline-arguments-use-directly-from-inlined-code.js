description(
"This tests that inlining preserves basic function.arguments functionality when said functionality is used directly from within an inlined code block."
);

function foo(a,b,c) {
    return foo.arguments;
}

function bar(a,b,c) {
    return foo(a,b,c);
}

function argsToStr(args) {
    var str = args + ": ";
    for (var i = 0; i < args.length; ++i) {
        if (i)
            str += ", ";
        str += args[i];
    }
    return str;
}

for (var __i = 0; __i < 200; ++__i)
    shouldBe("argsToStr(bar(\"a\" + __i, \"b\" + __i, \"c\" + __i))", "\"[object Arguments]: a" + __i + ", b" + __i + ", c" + __i + "\"");

