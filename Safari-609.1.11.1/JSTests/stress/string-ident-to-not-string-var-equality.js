function foo(a, b) {
    return a === b;
}

function bar(a, b) {
    return b === a;
}

function test(a, b, expected) {
    var fooActual = foo(a, b);
    var barActual = bar(a, b);
    
    if (fooActual != expected)
        throw new Error("Bad result: " + fooActual);
    if (barActual != expected)
        throw new Error("Bad result: " + barActual);
}

for (var i = 0; i < 10000; ++i) {
    test("foo", "foo", true);
    test("foo", "bar", false);
    test("fuz", 42, false);
    test("buz", {}, false);
    test("bla", null, false);
}

var fooString = "";
fooString += "f";
for (var i = 0; i < 2; ++i)
    fooString += "o";

test(fooString, "foo", true);
