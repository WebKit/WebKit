description(
"This tests that inlining preserves basic function.arguments functionality."
);

function foo() {
    return bar.arguments;
}

function bar(a,b,c) {
    return foo(a,b,c);
}

function baz(a,b,c) {
    return bar(a,b,c);
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
    shouldBe("argsToStr(baz(\"a\" + __i, \"b\" + __i, \"c\" + __i))", "\"[object Arguments]: a" + __i + ", b" + __i + ", c" + __i + "\"");

var successfullyParsed = true;
