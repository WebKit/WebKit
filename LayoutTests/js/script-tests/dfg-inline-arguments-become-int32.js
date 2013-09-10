description(
"This tests that inlining preserves function.arguments functionality if the arguments are reassigned to refer to an int32."
);

function foo() {
    return bar.arguments;
}

function bar(a,b,c) {
    b = 42;
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
    shouldBe("argsToStr(baz(\"a\" + __i, \"b\" + __i, \"c\" + __i))", "\"[object Arguments]: a" + __i + ", 42, c" + __i + "\"");

var successfullyParsed = true;
