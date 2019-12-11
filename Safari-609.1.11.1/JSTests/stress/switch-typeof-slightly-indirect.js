function foo(value) {
    var t = typeof value;
    if (!t)
        return -1;
    switch (t) {
    case "undefined":
        return 0;
    case "object":
        return 1;
    case "function":
        return 2;
    case "boolean":
        return 3;
    case "number":
        return 4;
    case "string":
        return 5;
    default:
        return 6;
    }
}

noInline(foo);

function test(value, expected) {
    var result = foo(value);
    if (result != expected)
        throw "Error: bad type code for " + value + ": " + result + " (expected " + expected + ")";
}

for (var i = 0; i < 10000; ++i) {
    test(void 0, 0);
    test({}, 1);
    test(function() { return 42; }, 2);
    test(true, 3);
    test(42, 4);
    test(42.5, 4);
    test("hello", 5);
}
