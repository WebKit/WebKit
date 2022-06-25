description(
"This tests that inlining preserves function.arguments functionality if the arguments are reassigned with a different type."
);

function foo() {
    return bar.arguments;
}

function bar(a,b,c) {
    b = a;
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
    shouldBe("argsToStr(baz(\"a\" + __i, __i + 2, \"c\" + __i))", "\"[object Arguments]: a" + __i + ", a" + __i + ", c" + __i + "\"");

var successfullyParsed = true;
