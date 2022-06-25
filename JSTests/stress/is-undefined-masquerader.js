function foo(o) {
    return typeof o === "undefined";
}

noInline(foo);

function test(o, expected) {
    var result = foo(o);
    if (result != expected)
        throw new Error("bad result: " + result);
}

for (var i = 0; i < 10000; ++i) {
    test(void 0, true);
    test(null, false);
    test(42, false);
    test({}, false);
    test("undefined", false);
    test(makeMasquerader(), true);
}
