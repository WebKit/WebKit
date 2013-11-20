function foo() {
    return a + b;
}

noInline(foo);

var a = 4;
var b = 5;

function check(actual, expected) {
    if (actual == expected)
        return;
    throw "Error: expected " + expected + " but got " + actual;
}

for (var i = 0; i < 100; ++i)
    check(foo(), 9);

a = 6;

for (var i = 0; i < 1000; ++i)
    check(foo(), 11);

b = 7;

for (var i = 0; i < 10000; ++i)
    check(foo(), 13);
