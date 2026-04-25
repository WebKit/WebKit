function foo(thingy) {
    if (thingy.length === void 0 || thingy.charAt !== void 0)
        return "yes";
    return "no";
}

noInline(foo);

function test(object, expected) {
    var result = foo(object);
    if (result != expected)
        throw new Error("Bad result: " + result);
}

for (var i = 0; i < 1000; ++i) {
    test({}, "yes");
    test([], "no");
    test("hello", "yes");
    test((function(){return arguments;})(), "no");
    var array = [];
    for (var j = 0; j < 100; ++j) {
        test(array, "no");
        array.push(42);
    }
}
