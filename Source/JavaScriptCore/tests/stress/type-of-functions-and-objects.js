function foo(v) {
    return typeof v;
}

function bar(v) {
    switch (typeof v) {
    case "object":
        return 1;
    case "function":
        return 2;
    default:
        return 3;
    }
}

function baz(v) {
    return typeof v == "function";
}

function fuzz(v) {
    return typeof v == "object";
}

noInline(foo);
noInline(bar);
noInline(baz);
noInline(fuzz);

function expect(f, v, expected) {
    var result = f(v);
    if (result != expected)
        throw "Error: " + f.name + "(" + v + ") returned " + result + " instead of " + expected;
}

function test(v, expected) {
    switch (expected) {
    case "function":
        expect(foo, v, "function");
        expect(bar, v, 2);
        expect(baz, v, true);
        expect(fuzz, v, false);
        break;
    case "object":
        expect(foo, v, "object");
        expect(bar, v, 1);
        expect(baz, v, false);
        expect(fuzz, v, true);
        break;
    case "other":
        var result = foo(v);
        if (result == "object" || result == "function")
            throw "Error: foo(" + v + ") returned " + result + " but expected something other than object or function";
        expect(bar, v, 3);
        expect(baz, v, false);
        expect(fuzz, v, false);
        break;
    default:
        throw "Bad expected case";
    }
}

for (var i = 0; i < 10000; ++i) {
    test({}, "object");
    test(function() { }, "function");
    test("hello", "other");
    test(42, "other");
    test(null, "object");
    test(void 0, "other");
    test(42.5, "other");
    test(Map, "function");
    test(Date, "function");
    test(Map.prototype, "object");
}
