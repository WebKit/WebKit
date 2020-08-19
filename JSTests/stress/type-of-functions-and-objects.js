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

function test() {
    var errors = [];

    function testValue(v, expected) {
        function expect(f, expected) {
            var result = f(v);
            if (result != expected)
                errors.push(f.name + "(" + v + ") returned " + result + " instead of " + expected);
        }

        switch (expected) {
        case "function":
            expect(foo, "function");
            expect(bar, 2);
            expect(baz, true);
            expect(fuzz, false);
            break;
        case "object":
            expect(foo, "object");
            expect(bar, 1);
            expect(baz, false);
            expect(fuzz, true);
            break;
        case "other":
            var result = foo(v);
            if (result == "object" || result == "function")
                errors.push("foo(" + v + ") returned " + result + " but expected something other than object or function");
            expect(bar, 3);
            expect(baz, false);
            expect(fuzz, false);
            break;
        default:
            throw "Bad expected case";
        }
    }
    
    testValue({}, "object");
    testValue(function() { }, "function");
    testValue("hello", "other");
    testValue(42, "other");
    testValue(null, "object");
    testValue(void 0, "other");
    testValue(42.5, "other");
    testValue(Map, "function");
    testValue(Date, "function");
    testValue(Map.prototype, "object");
    testValue(makeMasquerader(), "other");
    
    if (!errors.length)
        return;
    
    for (var i = 0; i < errors.length; ++i)
        print("Error: " + errors[i]);
    throw "Encountered errors during test run.";
}

for (var i = 0; i < 10000; ++i)
    test();

