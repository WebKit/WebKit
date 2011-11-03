description(
"This tests that inlining preserves function.arguments functionality if the arguments were represented as unboxed int32."
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
    shouldBe("argsToStr(baz(__i + 1, __i + 2, __i + 3))", "\"[object Arguments]: " + (__i + 1) + ", " + (__i + 2) + ", " + (__i + 3) + "\"");

var successfullyParsed = true;
