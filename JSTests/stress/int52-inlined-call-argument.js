function foo(a, b, c, p) {
    a = b + c;
    if (p) {
        a++;
        return a;
    }
}

function bar(a, b) {
    return foo("hello", a, b, a <= b);
}

noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = bar(2000000000, 2000000000);
    if (result != 4000000001)
        throw "Error: bad result: " + result;
}
