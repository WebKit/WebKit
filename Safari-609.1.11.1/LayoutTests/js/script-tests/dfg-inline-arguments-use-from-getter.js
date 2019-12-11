description(
"This tests that inlining preserves basic function.arguments functionality when said functionality is used from a getter."
);

function foo(o,b,c) {
    return o.f;
}

o = {}
o.__defineGetter__("f", function(){ return foo.arguments; });

function bar(o,b,c) {
    return foo(o,b,c);
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
    shouldBe("argsToStr(bar(o, \"b\" + __i, \"c\" + __i))", "\"[object Arguments]: [object Object], b" + __i + ", c" + __i + "\"");

