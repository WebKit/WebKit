function foo() {
    return a + b;
}

noInline(foo);

var a;
var b;

function setA(p, value) {
    if (p)
        a = value;
}

function setB(p, value) {
    if (p)
        b = value;
}

noInline(setA);
noInline(setB);

setA(true, 4);
setB(true, 5);

for (var i = 0; i < 1000; ++i) {
    setA(false, 42);
    setB(false, 42);
}

function check(actual, expected) {
    if (actual == expected)
        return;
    throw "Error: expected " + expected + " but got " + actual;
}

for (var i = 0; i < 100; ++i)
    check(foo(), 9);

setA(true, 6);

for (var i = 0; i < 1000; ++i)
    check(foo(), 11);

setB(true, 7);

for (var i = 0; i < 10000; ++i)
    check(foo(), 13);
