function foo(o) {
    return o == null;
}

noInline(foo);

function test(object, outcome) {
    var result = foo(object);
    if (result != outcome)
        throw new Error("Bad result: " + result);
}

for (var i = 0; i < 100000; ++i) {
    test(null, true);
    test({}, false);
    test(makeMasquerader(), true);
}
