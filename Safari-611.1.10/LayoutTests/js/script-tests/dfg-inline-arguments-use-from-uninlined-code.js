description(
"This tests that inlining preserves basic function.arguments functionality when said functionality is used from outside of the code where inlining actually happened."
);

function foo() {
    return bar.arguments;
}

function fuzz() {
    return baz.arguments;
}

function bar(a,b,c) {
    return foo(a,b,c);
}

function baz(a,b,c) {
    var array1 = bar(a,b,c);
    var array2 = fuzz(a,b,c);
    var result = [];
    for (var i = 0; i < array1.length; ++i)
        result.push(array1[i]);
    for (var i = 0; i < array2.length; ++i)
        result.push(array2[i]);
    return result;
}

for (var __i = 0; __i < 200; ++__i)
    shouldBe("\"\" + baz(\"a\" + __i, \"b\" + (__i + 1), \"c\" + (__i + 2))",
             "\"a" + __i + ",b" + (__i + 1) + ",c" + (__i + 2) + ",a" + __i + ",b" + (__i + 1) + ",c" + (__i + 2) + "\"");

var successfullyParsed = true;
