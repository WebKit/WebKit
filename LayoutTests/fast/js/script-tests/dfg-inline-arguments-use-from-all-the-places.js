description(
"This tests that inlining preserves basic function.arguments functionality when said functionality is used from inside and outside getters and from inlined code, all at once."
);

function foo(o,b,c) {
    return [foo.arguments, bar.arguments].concat(o.f);
}

function fuzz(a, b) {
    return [foo.arguments, bar.arguments, getter.arguments, fuzz.arguments];
}

function getter() {
    return [foo.arguments, bar.arguments, getter.arguments].concat(fuzz(42, 56));
}

o = {}
o.__defineGetter__("f", getter);

function bar(o,b,c) {
    return [bar.arguments].concat(foo(o,b,c));
}

function argsToStr(args) {
    if (args.length === void 0 || args.charAt !== void 0)
        return "" + args
    var str = "[" + args + ": ";
    for (var i = 0; i < args.length; ++i) {
        if (i)
            str += ", ";
        str += argsToStr(args[i]);
    }
    return str + "]";
}

for (var __i = 0; __i < 200; ++__i) {
    var text1 = "[[object Arguments]: [object Object], b" + __i + ", c" + __i + "]";
    var text2 = "[[object Arguments]: ]";
    var text3 = "[[object Arguments]: 42, 56]";
    shouldBe("argsToStr(bar(o, \"b\" + __i, \"c\" + __i))", "\"[[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments]: " + text1 + ", " + text1 + ", " + text1 + ", " + text1 + ", " + text1 + ", " + text2 + ", " + text1 + ", " + text1 + ", " + text2 + ", " + text3 + "]\"");
}

